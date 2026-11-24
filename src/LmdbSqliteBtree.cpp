/*
 * LMDB API subset (LmdbSqliteBtree.h) on top of the SQLite B-tree/pager/WAL layers,
 * accessed via LmdbSqliteBtreeShim.h. See the header for the mapping and the
 * differences to LMDB.
 */
#include "LmdbSqliteBtree.h"
#include "LmdbSqliteBtreeShim.h"
#include "ThreadSafety.h"

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace
{
    constexpr size_t max_dbis = 128;
    // dbi handles start at 2 (LMDB: 0 = free DB, 1 = main DB)
    constexpr unsigned dbi_offset = 2;
    constexpr size_t cursor_ring_size = 4;
    // application_id header field of the database file: "LMBT"
    constexpr uint32_t app_id = 0x4C4D4254;
    constexpr int cache_kib = 65536;
    constexpr int busy_timeout_ms = 10 * 60 * 1000;
    // Checkpoint after this many new WAL frames (pages) since the last trigger
    constexpr int ckpt_frames = 4096;

    // Catalog entry holding the transaction id. Names of dbis cannot contain NUL.
    const std::string txnid_key("\0txnid", 6);

    char empty_buf[1] = { 0 };

    thread_local int tl_err_code = 0;
    thread_local std::string tl_err_msg;
    thread_local std::string tl_strerror;

    int set_error(int code, const std::string& msg)
    {
        tl_err_code = code;
        tl_err_msg = msg;
        return code;
    }

    int bt_error(int rc)
    {
        int ret;
        switch (rc & 0xff)
        {
            case LSBT_FULL:
                ret = ENOSPC;
                break;
            case LSBT_IOERR:
                ret = EIO;
                break;
            case LSBT_NOMEM:
                ret = ENOMEM;
                break;
            case LSBT_CORRUPT:
            case LSBT_NOTADB:
                ret = MDB_CORRUPTED;
                break;
            case LSBT_READONLY:
            case LSBT_PERM:
                ret = EACCES;
                break;
            case LSBT_BUSY:
            case LSBT_LOCKED:
                ret = EBUSY;
                break;
            case LSBT_CANTOPEN:
                ret = ENOENT;
                break;
            case LSBT_TOOBIG:
                ret = MDB_BAD_VALSIZE;
                break;
            case LSBT_NOTFOUND:
                ret = MDB_PAGE_NOTFOUND;
                break;
            default:
                ret = MDB_PANIC;
                break;
        }
        return set_error(ret, std::string("SQLite btree: ") + lsbt_errstr(rc));
    }

    // Maps LSBT_DONE to MDB_NOTFOUND and other errors via bt_error()
    int bt_result(int rc)
    {
        if (rc == LSBT_OK)
            return MDB_SUCCESS;
        if (rc == LSBT_DONE)
            return MDB_NOTFOUND;
        return bt_error(rc);
    }

    int memn_cmp(const void* a, size_t na, const void* b, size_t nb)
    {
        size_t len = na < nb ? na : nb;
        int diff = len > 0 ? memcmp(a, b, len) : 0;
        if (diff != 0)
            return diff;
        return na < nb ? -1 : (na > nb ? 1 : 0);
    }

    void put_u32(char* p, uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            p[i] = static_cast<char>((v >> (8 * i)) & 0xff);
    }

    uint32_t get_u32(const char* p)
    {
        uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= static_cast<uint32_t>(static_cast<unsigned char>(p[i])) << (8 * i);
        return v;
    }

    struct DbiInfo
    {
        std::string name;
        std::atomic<MDB_cmp_func*> cmp{ nullptr };
        std::atomic<MDB_cmp_func*> dcmp{ nullptr };
        std::atomic<bool> opened{ false };
        std::atomic<uint32_t> root{ 0 };
        // Read transactions with an older txnid do not have the B-tree
        std::atomic<size_t> created_txnid{ 0 };
        // Set under MDB_env::mutex before opened is set, read only after opened
        bool dupsort = false;
    };

    int collation_cmp(MDB_cmp_func* f, int na, const void* a, int nb, const void* b)
    {
        if (f == nullptr)
            return memn_cmp(a, static_cast<size_t>(na), b, static_cast<size_t>(nb));

        MDB_val va{ static_cast<size_t>(na), const_cast<void*>(a) };
        MDB_val vb{ static_cast<size_t>(nb), const_cast<void*>(b) };
        return f(&va, &vb);
    }

    int key_collation(void* p, int na, const void* a, int nb, const void* b)
    {
        return collation_cmp(static_cast<DbiInfo*>(p)->cmp.load(std::memory_order_relaxed), na, a, nb, b);
    }

    int dup_collation(void* p, int na, const void* a, int nb, const void* b)
    {
        return collation_cmp(static_cast<DbiInfo*>(p)->dcmp.load(std::memory_order_relaxed), na, a, nb, b);
    }

    struct Conn
    {
        lsbt_conn* c = nullptr;
        // Per dbi slot, created on first use on this connection
        std::vector<lsbt_keyinfo*> kinfo;
        std::vector<bool> kinfo_dupsort;
        lsbt_keyinfo* catalog_kinfo = nullptr;

        Conn() : kinfo(max_dbis, nullptr), kinfo_dupsort(max_dbis, false) {}

        ~Conn()
        {
            for (lsbt_keyinfo* ki : kinfo)
                lsbt_keyinfo_free(ki);
            lsbt_keyinfo_free(catalog_kinfo);
            lsbt_close(c);
        }

        Conn(const Conn&) = delete;
        Conn& operator=(const Conn&) = delete;
    };

    struct Entry
    {
        std::vector<char> k;
        std::vector<char> v;
    };

    void assign(std::vector<char>& out, const void* p, size_t n)
    {
        const char* cp = static_cast<const char*>(p);
        out.assign(cp, cp + n);
    }

    void set_val(MDB_val* out, std::vector<char>& v)
    {
        out->mv_size = v.size();
        out->mv_data = v.empty() ? empty_buf : v.data();
    }

    const void* val_data(const MDB_val* v)
    {
        return v->mv_data != nullptr ? v->mv_data : empty_buf;
    }

} // namespace

struct MDB_env
{
    std::string path;
    std::string file_path;
    unsigned int flags = 0;
    bool opened = false;
    uint32_t catalog_root = 0;

    std::mutex mutex; // protects dbi registration and the reader pool
    // Set (under mutex) once per slot, read without lock
    std::atomic<DbiInfo*> dbis[max_dbis];
    std::vector<std::unique_ptr<DbiInfo>> dbi_storage GUARDED_BY(mutex);
    std::vector<std::unique_ptr<Conn>> free_readers GUARDED_BY(mutex);

    // Only used by the write transaction holder (write_busy) and at open/close
    std::unique_ptr<Conn> writer;
    std::mutex write_mutex;
    std::condition_variable write_cond;
    bool write_busy GUARDED_BY(write_mutex) = false;

    std::atomic<size_t> last_txnid{ 0 };

    // WAL checkpoints run in a separate thread on their own connection. Held while a
    // checkpoint or mdb_env_sync() runs.
    std::mutex ckpt_run_mutex;
    std::unique_ptr<Conn> ckpt_conn GUARDED_BY(ckpt_run_mutex);
    std::thread ckpt_thread;
    std::mutex ckpt_mutex;
    std::condition_variable ckpt_cond;
    bool ckpt_stop GUARDED_BY(ckpt_mutex) = false;
    bool ckpt_pending GUARDED_BY(ckpt_mutex) = false;
    // Only used by the write transaction holder
    int ckpt_trigger_frames = 0;
    std::atomic<uint64_t> commits{ 0 };

    MDB_env()
    {
        for (auto& d : dbis)
            d.store(nullptr);
    }
};

namespace
{
    void ckpt_loop(MDB_env* env) EXCLUDES(env->ckpt_mutex, env->ckpt_run_mutex)
    {
        uint64_t done_commits = env->commits.load();
        bool complete = true;
        CondVarLock lock(env->ckpt_mutex);
        while (!env->ckpt_stop)
        {
            if (!env->ckpt_pending)
                lock.waitFor(env->ckpt_cond, std::chrono::seconds(1));
            if (env->ckpt_stop)
                break;

            bool pending = env->ckpt_pending;
            env->ckpt_pending = false;
            uint64_t commits = env->commits.load();
            // Without trigger, checkpoint when idle (no commit for a second) and not everything is checkpointed
            if (!pending && (commits != done_commits || complete))
            {
                done_commits = commits;
                continue;
            }

            lock.unlock();
            {
                std::scoped_lock run_lock(env->ckpt_run_mutex);
                int n_log = 0;
                int n_ckpt = 0;
                int rc = lsbt_checkpoint(env->ckpt_conn->c, &n_log, &n_ckpt);
                complete = rc == LSBT_OK && n_ckpt >= n_log;
            }
            done_commits = commits;
            lock.lock();
        }
    }

    // Called by the write transaction holder after a commit
    void after_commit(MDB_env* env)
    {
        env->commits.fetch_add(1, std::memory_order_relaxed);
        int frames = lsbt_wal_frames(env->writer->c);
        if (frames <= 0)
            return;
        if (frames < env->ckpt_trigger_frames)
            env->ckpt_trigger_frames = 0; // WAL was restarted
        if (frames - env->ckpt_trigger_frames >= ckpt_frames)
        {
            env->ckpt_trigger_frames = frames;
            {
                std::scoped_lock lock(env->ckpt_mutex);
                env->ckpt_pending = true;
            }
            env->ckpt_cond.notify_one();
        }
    }

    void stop_ckpt_thread(MDB_env* env) EXCLUDES(env->ckpt_mutex)
    {
        if (!env->ckpt_thread.joinable())
            return;
        {
            std::scoped_lock lock(env->ckpt_mutex);
            env->ckpt_stop = true;
        }
        env->ckpt_cond.notify_one();
        env->ckpt_thread.join();
    }

    void close_env_conns(MDB_env* env) EXCLUDES(env->mutex, env->ckpt_mutex, env->ckpt_run_mutex)
    {
        stop_ckpt_thread(env);
        {
            std::scoped_lock lock(env->mutex);
            env->free_readers.clear();
        }
        {
            std::scoped_lock lock(env->ckpt_run_mutex);
            env->ckpt_conn.reset();
        }
        // Last: closing the last connection checkpoints and removes the WAL
        env->writer.reset();
    }
} // namespace

struct MDB_txn
{
    MDB_env* env = nullptr;
    Conn* conn = nullptr;
    std::unique_ptr<Conn> reader;
    bool rdonly = false;
    bool dirty = false;
    // Incremented on each modification (invalidates the page caches of cursors)
    uint64_t mods = 0;
    size_t txnid = 0;
    std::vector<MDB_cursor*> cursors;
    // B-tree cursors for mdb_get/mdb_put/mdb_del, per dbi slot
    std::vector<lsbt_cursor*> tmp_cursors;
    // dbis created in this (write) transaction
    std::vector<DbiInfo*> created;
    // Result of the last mdb_get() (or of an MDB_NOOVERWRITE put that found the key)
    std::vector<char> get_buf;
    std::vector<std::vector<char>> retained;

    void close_tmp_cursors()
    {
        for (lsbt_cursor*& c : tmp_cursors)
        {
            lsbt_cursor_close(c);
            c = nullptr;
        }
    }
};

struct MDB_cursor
{
    MDB_txn* txn = nullptr;
    DbiInfo* d = nullptr;
    unsigned slot = 0;
    lsbt_cursor* bc = nullptr;

    Entry ring[cursor_ring_size];
    size_t cur = 0;
    size_t next = 0;
    bool positioned = false;
    bool deleted = false;

    uint32_t page_no = 0;
    uint64_t page_mods = 0;
    std::vector<Entry> page;
    size_t page_size = 0;

    Entry& current() { return ring[cur]; }

    Entry& new_slot()
    {
        cur = next;
        next = (next + 1) % cursor_ring_size;
        positioned = true;
        deleted = false;
        return ring[cur];
    }

    void store_kv(const void* k, size_t nk, const void* v, size_t nv)
    {
        // Input may point into one of the ring entries
        std::vector<char> tk(static_cast<const char*>(k), static_cast<const char*>(k) + nk);
        std::vector<char> tv(static_cast<const char*>(v), static_cast<const char*>(v) + nv);
        Entry& e = new_slot();
        e.k.swap(tk);
        e.v.swap(tv);
    }

    void close_bc()
    {
        lsbt_cursor_close(bc);
        bc = nullptr;
    }
};

namespace
{
    int open_conn(MDB_env* env, bool create, std::unique_ptr<Conn>& out)
    {
        auto conn = std::make_unique<Conn>();
        int rc = lsbt_open(env->file_path.c_str(), create ? 1 : 0, &conn->c);
        if (rc != LSBT_OK)
        {
            int ret = bt_error(rc);
            tl_err_msg += " (" + env->file_path + ")";
            return ret;
        }

        // synchronous=NORMAL: without MDB_NOSYNC end_txn() fsyncs the WAL after each commit
        lsbt_configure(conn->c, 0, cache_kib, busy_timeout_ms);

        conn->catalog_kinfo = lsbt_keyinfo_create(conn->c, 0, nullptr, nullptr, nullptr, nullptr);
        if (conn->catalog_kinfo == nullptr)
            return set_error(ENOMEM, "Allocating key info failed");

        out = std::move(conn);
        return MDB_SUCCESS;
    }

    DbiInfo* find_or_add_dbi(MDB_env* env, const std::string& name, unsigned* slot_out) EXCLUDES(env->mutex)
    {
        std::scoped_lock lock(env->mutex);
        for (unsigned i = 0; i < max_dbis; ++i)
        {
            DbiInfo* d = env->dbis[i].load(std::memory_order_relaxed);
            if (d == nullptr)
            {
                auto nd = std::make_unique<DbiInfo>();
                nd->name = name;
                d = nd.get();
                env->dbi_storage.push_back(std::move(nd));
                env->dbis[i].store(d, std::memory_order_release);
                *slot_out = i;
                return d;
            }
            if (d->name == name)
            {
                *slot_out = i;
                return d;
            }
        }
        return nullptr;
    }

    DbiInfo* get_dbi(MDB_txn* txn, MDB_dbi dbi, unsigned* slot)
    {
        if (txn == nullptr || dbi < dbi_offset || dbi >= dbi_offset + max_dbis)
            return nullptr;
        *slot = dbi - dbi_offset;
        DbiInfo* d = txn->env->dbis[*slot].load(std::memory_order_acquire);
        if (d == nullptr || !d->opened.load(std::memory_order_acquire))
            return nullptr;
        return d;
    }

    int open_catalog_cursor(MDB_txn* txn, lsbt_cursor** out)
    {
        int rc = lsbt_cursor_open(txn->conn->c, txn->env->catalog_root, txn->rdonly ? 0 : 1, txn->conn->catalog_kinfo, out);
        return rc == LSBT_OK ? MDB_SUCCESS : bt_error(rc);
    }

    // Looks up a catalog entry, copying its value to *out. MDB_NOTFOUND if missing.
    int catalog_get(MDB_txn* txn, const std::string& key, std::string* out)
    {
        lsbt_cursor* cur;
        int rc = open_catalog_cursor(txn, &cur);
        if (rc)
            return rc;

        int eq = 0;
        int brc = lsbt_seek(cur, key.data(), key.size(), nullptr, 0, LSBT_SEEK_GE, &eq);
        if (brc == LSBT_OK && eq)
        {
            lsbt_kv kv;
            brc = lsbt_read(cur, &kv);
            if (brc == LSBT_OK)
                out->assign(static_cast<const char*>(kv.v), kv.nv);
        }
        else if (brc == LSBT_OK)
        {
            brc = LSBT_DONE;
        }
        lsbt_cursor_close(cur);
        return bt_result(brc);
    }

    int catalog_put(MDB_txn* txn, const std::string& key, const std::string& value)
    {
        lsbt_cursor* cur;
        int rc = open_catalog_cursor(txn, &cur);
        if (rc)
            return rc;
        int brc = lsbt_put(cur, key.data(), key.size(), value.data(), value.size());
        lsbt_cursor_close(cur);
        return bt_result(brc);
    }

    int read_txnid(MDB_txn* txn, size_t* txnid)
    {
        std::string v;
        int rc = catalog_get(txn, txnid_key, &v);
        if (rc == MDB_NOTFOUND || (rc == MDB_SUCCESS && v.size() != 8))
            return set_error(MDB_CORRUPTED, "LMDB-SQLite-btree meta data missing");
        if (rc)
            return rc;
        uint64_t t = get_u32(v.data()) | (static_cast<uint64_t>(get_u32(v.data() + 4)) << 32);
        *txnid = static_cast<size_t>(t);
        return MDB_SUCCESS;
    }

    int write_txnid(MDB_txn* txn, size_t txnid)
    {
        std::string v(8, '\0');
        uint64_t t = txnid;
        put_u32(v.data(), static_cast<uint32_t>(t));
        put_u32(v.data() + 4, static_cast<uint32_t>(t >> 32));
        return catalog_put(txn, txnid_key, v);
    }

    lsbt_keyinfo* get_kinfo(Conn* conn, unsigned slot, DbiInfo* d)
    {
        if (conn->kinfo[slot] != nullptr && conn->kinfo_dupsort[slot] != d->dupsort)
        {
            // The dbi was closed (aborted creation) and recreated with other flags
            lsbt_keyinfo_free(conn->kinfo[slot]);
            conn->kinfo[slot] = nullptr;
        }
        if (conn->kinfo[slot] == nullptr)
        {
            conn->kinfo[slot] = lsbt_keyinfo_create(conn->c, d->dupsort ? 1 : 0, key_collation, d, dup_collation, d);
            conn->kinfo_dupsort[slot] = d->dupsort;
        }
        return conn->kinfo[slot];
    }

    int open_bc(MDB_txn* txn, unsigned slot, DbiInfo* d, lsbt_cursor** out)
    {
        if (txn->txnid < d->created_txnid.load(std::memory_order_relaxed))
            return set_error(MDB_BAD_DBI, "Database " + d->name + " was created after the start of the read transaction");
        lsbt_keyinfo* ki = get_kinfo(txn->conn, slot, d);
        if (ki == nullptr)
            return set_error(ENOMEM, "Allocating key info failed");
        int rc = lsbt_cursor_open(txn->conn->c, d->root.load(std::memory_order_relaxed), txn->rdonly ? 0 : 1, ki, out);
        return rc == LSBT_OK ? MDB_SUCCESS : bt_error(rc);
    }

    int tmp_cursor(MDB_txn* txn, unsigned slot, DbiInfo* d, lsbt_cursor** out)
    {
        if (txn->tmp_cursors.empty())
            txn->tmp_cursors.resize(max_dbis, nullptr);
        if (txn->tmp_cursors[slot] == nullptr)
        {
            int rc = open_bc(txn, slot, d, &txn->tmp_cursors[slot]);
            if (rc)
                return rc;
        }
        *out = txn->tmp_cursors[slot];
        return MDB_SUCCESS;
    }

    void modified(MDB_txn* txn)
    {
        txn->dirty = true;
        ++txn->mods;
    }

    // Position on the entry with the key (first duplicate for dupsort) or, if data is
    // given and the dbi is dupsort, on the key/data pair. MDB_NOTFOUND if missing.
    int seek_exact(lsbt_cursor* bc, DbiInfo* d, const MDB_val* key, const MDB_val* data)
    {
        const bool pair = data != nullptr && d->dupsort;
        int eq = 0;
        int rc = lsbt_seek(bc, val_data(key), key->mv_size, pair ? val_data(data) : nullptr, pair ? data->mv_size : 0,
                           LSBT_SEEK_GE, &eq);
        if (rc == LSBT_OK && !eq)
            return MDB_NOTFOUND;
        return bt_result(rc);
    }

    int seek_entry(lsbt_cursor* bc, DbiInfo* d, Entry& e)
    {
        MDB_val k, v;
        set_val(&k, e.k);
        set_val(&v, e.v);
        return seek_exact(bc, d, &k, &v);
    }

    int do_put(MDB_txn* txn, lsbt_cursor* bc, DbiInfo* d, MDB_val* key, MDB_val* data, unsigned int flags)
    {
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        if (key->mv_size == 0)
            return MDB_BAD_VALSIZE;
        if (flags & (MDB_RESERVE | MDB_MULTIPLE))
            return set_error(EINVAL, "MDB_RESERVE/MDB_MULTIPLE not supported");

        if (flags & MDB_NOOVERWRITE)
        {
            int rc = seek_exact(bc, d, key, nullptr);
            if (rc == MDB_SUCCESS)
            {
                lsbt_kv kv;
                int brc = lsbt_read(bc, &kv);
                if (brc != LSBT_OK)
                    return bt_result(brc);
                assign(txn->get_buf, kv.v, kv.nv);
                data->mv_size = txn->get_buf.size();
                data->mv_data = txn->get_buf.empty() ? empty_buf : txn->get_buf.data();
                return MDB_KEYEXIST;
            }
            if (rc != MDB_NOTFOUND)
                return rc;
        }
        else if (d->dupsort && (flags & MDB_NODUPDATA))
        {
            int rc = seek_exact(bc, d, key, data);
            if (rc == MDB_SUCCESS)
                return MDB_KEYEXIST;
            if (rc != MDB_NOTFOUND)
                return rc;
        }

        modified(txn);
        int rc = lsbt_put(bc, val_data(key), key->mv_size, val_data(data), data->mv_size);
        return rc == LSBT_OK ? MDB_SUCCESS : bt_error(rc);
    }

    // Deletes all duplicates of the key (dupsort) or the key
    int delete_key(MDB_txn* txn, lsbt_cursor* bc, DbiInfo* d, const MDB_val* key)
    {
        bool found = false;
        for (;;)
        {
            int rc = seek_exact(bc, d, key, nullptr);
            if (rc == MDB_NOTFOUND)
                break;
            if (rc)
                return rc;
            modified(txn);
            int brc = lsbt_delete(bc);
            if (brc != LSBT_OK)
                return bt_error(brc);
            found = true;
            if (!d->dupsort)
                break;
        }
        return found ? MDB_SUCCESS : MDB_NOTFOUND;
    }

    void detach_cursor(MDB_cursor* c)
    {
        c->close_bc();
        c->txn = nullptr;
    }

    void revert_created(MDB_txn* txn) EXCLUDES(txn->env->mutex)
    {
        std::scoped_lock lock(txn->env->mutex);
        for (DbiInfo* d : txn->created)
        {
            d->opened.store(false, std::memory_order_release);
            d->root.store(0, std::memory_order_relaxed);
        }
        txn->created.clear();
    }

    int end_txn(MDB_txn* txn, bool commit)
    {
        if (txn == nullptr)
            return EINVAL;

        // B-tree cursors must be closed before the transaction ends
        for (MDB_cursor* c : txn->cursors)
        {
            detach_cursor(c);
            // LMDB frees cursors of write transactions at transaction end
            if (!txn->rdonly)
                delete c;
        }
        txn->cursors.clear();
        txn->close_tmp_cursors();

        MDB_env* env = txn->env;
        int ret = MDB_SUCCESS;

        if (!txn->rdonly)
        {
            if (commit)
            {
                if (txn->dirty)
                    ret = write_txnid(txn, txn->txnid);
                if (!ret)
                {
                    int rc = lsbt_commit(txn->conn->c);
                    if (rc != LSBT_OK)
                        ret = bt_error(rc);
                }
                if (ret)
                {
                    std::string msg = tl_err_msg;
                    lsbt_rollback(txn->conn->c);
                    revert_created(txn);
                    set_error(ret, msg);
                }
                else if (txn->dirty)
                {
                    env->last_txnid.store(txn->txnid);
                    after_commit(env);
                    if (!(env->flags & MDB_NOSYNC))
                    {
                        int rc = lsbt_sync_wal(txn->conn->c);
                        if (rc != LSBT_OK)
                        {
                            ret = bt_error(rc);
                            tl_err_msg = "fsync of " + env->file_path + "-wal failed (" + tl_err_msg + ")";
                        }
                    }
                }
            }
            else
            {
                lsbt_rollback(txn->conn->c);
                revert_created(txn);
            }

            {
                std::scoped_lock lock(env->write_mutex);
                env->write_busy = false;
            }
            env->write_cond.notify_one();
        }
        else
        {
            int rc = lsbt_commit(txn->conn->c);
            if (rc != LSBT_OK)
            {
                lsbt_rollback(txn->conn->c);
                txn->reader.reset();
            }
            else
            {
                std::scoped_lock lock(env->mutex);
                env->free_readers.push_back(std::move(txn->reader));
            }
        }

        delete txn;
        return ret;
    }

    int store_current(MDB_cursor* c)
    {
        lsbt_kv kv;
        int rc = lsbt_read(c->bc, &kv);
        if (rc != LSBT_OK)
            return bt_result(rc);
        Entry& e = c->new_slot();
        assign(e.k, kv.k, kv.nk);
        assign(e.v, kv.v, kv.nv);
        return MDB_SUCCESS;
    }

    // Result of a cursor positioning: on success stores the entry as current
    int cursor_result(MDB_cursor* c, int brc)
    {
        if (brc != LSBT_OK)
            return bt_result(brc);
        return store_current(c);
    }

    int cursor_move(MDB_cursor* c, int dir)
    {
        if (!c->positioned)
            return cursor_result(c, dir > 0 ? lsbt_first(c->bc) : lsbt_last(c->bc));

        int brc;
        if (lsbt_positioned(c->bc))
        {
            brc = dir > 0 ? lsbt_next(c->bc) : lsbt_prev(c->bc);
        }
        else
        {
            // B-tree cursor lost its position (moved past the end before): seek from the current entry
            Entry pos = c->current();
            const bool pair = c->d->dupsort;
            brc = lsbt_seek(c->bc, pos.k.empty() ? empty_buf : pos.k.data(), pos.k.size(), pair ? (pos.v.empty() ? empty_buf : pos.v.data()) : nullptr,
                            pair ? pos.v.size() : 0, dir > 0 ? LSBT_SEEK_GT : LSBT_SEEK_LT, nullptr);
        }
        return cursor_result(c, brc);
    }

    void page_cb(void* arg, unsigned idx, const lsbt_kv* kv)
    {
        MDB_cursor* c = static_cast<MDB_cursor*>(arg);
        if (c->page.size() <= idx)
            c->page.resize(idx + 1);
        assign(c->page[idx].k, kv->k, kv->nk);
        assign(c->page[idx].v, kv->v, kv->nv);
    }

    int load_page(MDB_cursor* c, size_t pgno)
    {
        if (c->txn == nullptr)
            return EINVAL;
        if (pgno == 0 || pgno > UINT32_MAX)
            return MDB_PAGE_NOTFOUND;
        if (c->page_no == pgno && c->page_mods == c->txn->mods)
            return MDB_SUCCESS;

        c->page_no = 0;
        unsigned n = 0;
        int rc = lsbt_read_page(c->txn->conn->c, static_cast<uint32_t>(pgno), page_cb, c, &n);
        if (rc != LSBT_OK)
            return bt_error(rc);
        c->page_size = n;
        c->page_no = static_cast<uint32_t>(pgno);
        c->page_mods = c->txn->mods;
        return MDB_SUCCESS;
    }

    int cursor_pgno(MDB_cursor* c, size_t* pgno)
    {
        uint32_t p = 0;
        int rc = lsbt_cursor_pgno(c->bc, &p);
        if (rc == LSBT_DONE)
            return EINVAL;
        if (rc != LSBT_OK)
            return bt_error(rc);
        *pgno = p;
        return MDB_SUCCESS;
    }

    const char* lmdb_errstr(int err)
    {
        static const char* const errs[] = {
            "MDB_KEYEXIST: Key/data pair already exists",
            "MDB_NOTFOUND: No matching key/data pair found",
            "MDB_PAGE_NOTFOUND: Requested page not found",
            "MDB_CORRUPTED: Located page was wrong type",
            "MDB_PANIC: Update of meta page failed or environment had fatal error",
            "MDB_VERSION_MISMATCH: Database environment version mismatch",
            "MDB_INVALID: File is not an LMDB file",
            "MDB_MAP_FULL: Environment mapsize limit reached",
            "MDB_DBS_FULL: Environment maxdbs limit reached",
            "MDB_READERS_FULL: Environment maxreaders limit reached",
            "MDB_TLS_FULL: Thread-local storage keys full - too many environments open",
            "MDB_TXN_FULL: Transaction has too many dirty pages - transaction too big",
            "MDB_CURSOR_FULL: Internal error - cursor stack limit reached",
            "MDB_PAGE_FULL: Internal error - page has no more space",
            "MDB_MAP_RESIZED: Database contents grew beyond environment mapsize",
            "MDB_INCOMPATIBLE: Operation and DB incompatible, or DB flags changed",
            "MDB_BAD_RSLOT: Invalid reuse of reader locktable slot",
            "MDB_BAD_TXN: Transaction must abort, has a child, or is invalid",
            "MDB_BAD_VALSIZE: Unsupported size of key/DB name/data, or wrong DUPFIXED size",
            "MDB_BAD_DBI: The specified DBI handle was closed/changed unexpectedly",
        };
        if (err >= MDB_KEYEXIST && err <= MDB_LAST_ERRCODE)
            return errs[err - MDB_KEYEXIST];
        return nullptr;
    }

    // Sets up a new database file or checks an existing one. Returns the catalog root.
    int init_db(MDB_env* env, uint32_t* catalog_root)
    {
        lsbt_conn* c = env->writer->c;
        int rc = lsbt_enable_wal(c);
        if (rc != LSBT_OK)
        {
            if ((rc & 0xff) == LSBT_NOTADB)
                return set_error(MDB_INVALID, env->file_path + " is not an SQLite database (LMDB database?)");
            return bt_error(rc);
        }

        rc = lsbt_begin(c, 1);
        if (rc != LSBT_OK)
            return bt_error(rc);

        uint32_t aid = lsbt_get_meta(c, LSBT_META_APPLICATION_ID);
        uint32_t root = lsbt_get_meta(c, LSBT_META_USER_VERSION);
        if (aid == app_id && root >= 2)
        {
            lsbt_rollback(c);
            *catalog_root = root;
            return MDB_SUCCESS;
        }
        if (aid != 0 || root != 0 || lsbt_page_count(c) > 1)
        {
            lsbt_rollback(c);
            return set_error(MDB_INVALID, env->file_path + " is not an LMDB-SQLite-btree database (created by the SQL based backend?)");
        }

        // New database: catalog B-tree with the initial transaction id
        rc = lsbt_create_tree(c, &root);
        if (rc == LSBT_OK)
            rc = lsbt_update_meta(c, LSBT_META_USER_VERSION, root);
        if (rc == LSBT_OK)
            rc = lsbt_update_meta(c, LSBT_META_APPLICATION_ID, app_id);
        if (rc != LSBT_OK)
        {
            lsbt_rollback(c);
            return bt_error(rc);
        }

        env->catalog_root = root;
        MDB_txn tmp;
        tmp.env = env;
        tmp.conn = env->writer.get();
        int ret = write_txnid(&tmp, 0);
        if (ret)
        {
            lsbt_rollback(c);
            return ret;
        }
        rc = lsbt_commit(c);
        if (rc != LSBT_OK)
        {
            lsbt_rollback(c);
            return bt_error(rc);
        }
        *catalog_root = root;
        return MDB_SUCCESS;
    }

} // namespace

extern "C"
{

    char* mdb_strerror(int err)
    {
        if (err == 0)
        {
            tl_strerror = "Successful return: 0";
        }
        else
        {
            const char* s = lmdb_errstr(err);
            tl_strerror = s != nullptr ? s : strerror(err);
            if (err == tl_err_code && !tl_err_msg.empty())
                tl_strerror += " (" + tl_err_msg + ")";
        }
        return tl_strerror.data();
    }

    int mdb_env_create(MDB_env** env)
    {
        if (env == nullptr)
            return EINVAL;
        *env = new MDB_env;
        return MDB_SUCCESS;
    }

    int mdb_env_open(MDB_env* env, const char* path, unsigned int flags, mdb_mode_t /*mode*/)
    {
        if (env == nullptr || path == nullptr || env->opened)
            return EINVAL;

        env->path = path;
        env->flags = flags;
        env->file_path = (flags & MDB_NOSUBDIR) ? env->path : env->path + "/data.mdb";

        int rc = open_conn(env, true, env->writer);
        if (rc)
            return rc;

        uint32_t catalog_root = 0;
        rc = init_db(env, &catalog_root);
        if (rc)
        {
            env->writer.reset();
            return rc;
        }
        env->catalog_root = catalog_root;

        size_t txnid = 0;
        {
            int brc = lsbt_begin(env->writer->c, 0);
            if (brc != LSBT_OK)
            {
                rc = bt_error(brc);
                env->writer.reset();
                return rc;
            }
            MDB_txn tmp;
            tmp.env = env;
            tmp.conn = env->writer.get();
            tmp.rdonly = true;
            rc = read_txnid(&tmp, &txnid);
            lsbt_commit(env->writer->c);
            if (rc)
            {
                env->writer.reset();
                return rc;
            }
        }

        std::unique_ptr<Conn> ckpt_conn;
        rc = open_conn(env, false, ckpt_conn);
        if (!rc)
        {
            // Opens the WAL of the connection (a checkpoint without would run SQL to do so)
            int brc = lsbt_begin(ckpt_conn->c, 0);
            if (brc == LSBT_OK)
                brc = lsbt_commit(ckpt_conn->c);
            if (brc != LSBT_OK)
                rc = bt_error(brc);
        }
        if (rc)
        {
            ckpt_conn.reset();
            close_env_conns(env);
            return rc;
        }
        {
            std::scoped_lock lock(env->ckpt_run_mutex);
            env->ckpt_conn = std::move(ckpt_conn);
        }
        env->ckpt_thread = std::thread(ckpt_loop, env);

        env->last_txnid.store(txnid);
        env->opened = true;
        return MDB_SUCCESS;
    }

    void mdb_env_close(MDB_env* env)
    {
        if (env == nullptr)
            return;
        close_env_conns(env);
        delete env;
    }

    int mdb_env_sync(MDB_env* env, int force)
    {
        if (env == nullptr || !env->opened)
            return EINVAL;

        if (!force && (env->flags & MDB_NOSYNC))
            return MDB_SUCCESS;

        // Committed data is in the WAL until a checkpoint copied it into the database file
        // (and synced it, before the WAL may be restarted). So fsyncing the WAL, without a
        // checkpoint running concurrently, makes all commits durable.
        std::scoped_lock lock(env->ckpt_run_mutex);
        int rc = lsbt_sync_wal(env->ckpt_conn->c);
        if (rc != LSBT_OK)
        {
            int ret = bt_error(rc);
            tl_err_msg = "fsync of " + env->file_path + "-wal failed (" + tl_err_msg + ")";
            return ret;
        }
        return MDB_SUCCESS;
    }

    int mdb_env_get_path(MDB_env* env, const char** path)
    {
        if (env == nullptr || path == nullptr)
            return EINVAL;
        *path = env->path.c_str();
        return MDB_SUCCESS;
    }

    int mdb_env_set_mapsize(MDB_env* env, size_t /*size*/)
    {
        return env != nullptr ? MDB_SUCCESS : EINVAL;
    }

    int mdb_env_set_maxreaders(MDB_env* env, unsigned int /*readers*/)
    {
        return env != nullptr ? MDB_SUCCESS : EINVAL;
    }

    int mdb_env_set_maxdbs(MDB_env* env, MDB_dbi dbs)
    {
        if (env == nullptr || dbs > max_dbis)
            return EINVAL;
        return MDB_SUCCESS;
    }

    int mdb_env_set_idl_dirty_max(MDB_env* env, unsigned int /*idl_dirty_max*/)
    {
        return env != nullptr ? MDB_SUCCESS : EINVAL;
    }

    int mdb_txn_begin(MDB_env* env, MDB_txn* parent, unsigned int flags, MDB_txn** out)
    {
        if (env == nullptr || out == nullptr || !env->opened)
            return EINVAL;
        if (parent != nullptr)
            return set_error(EINVAL, "Nested transactions are not supported");

        auto txn = std::make_unique<MDB_txn>();
        txn->env = env;
        txn->rdonly = (flags & MDB_RDONLY) != 0;

        if (txn->rdonly)
        {
            {
                std::scoped_lock lock(env->mutex);
                if (!env->free_readers.empty())
                {
                    txn->reader = std::move(env->free_readers.back());
                    env->free_readers.pop_back();
                }
            }
            if (!txn->reader)
            {
                int rc = open_conn(env, false, txn->reader);
                if (rc)
                    return rc;
            }
            txn->conn = txn->reader.get();

            // Starts the read snapshot
            int brc = lsbt_begin(txn->conn->c, 0);
            if (brc != LSBT_OK)
                return bt_error(brc);
            int rc = read_txnid(txn.get(), &txn->txnid);
            if (rc)
            {
                lsbt_rollback(txn->conn->c);
                return rc;
            }
        }
        else
        {
            {
                CondVarLock lock(env->write_mutex);
                while (env->write_busy)
                    lock.wait(env->write_cond);
                env->write_busy = true;
            }
            txn->conn = env->writer.get();

            int rc = MDB_SUCCESS;
            int brc = lsbt_begin(txn->conn->c, 1);
            size_t txnid = 0;
            if (brc != LSBT_OK)
            {
                rc = bt_error(brc);
            }
            else
            {
                rc = read_txnid(txn.get(), &txnid);
                if (rc)
                    lsbt_rollback(txn->conn->c);
            }
            if (rc)
            {
                {
                    std::scoped_lock lock(env->write_mutex);
                    env->write_busy = false;
                }
                env->write_cond.notify_one();
                return rc;
            }
            txn->txnid = txnid + 1;
        }

        *out = txn.release();
        return MDB_SUCCESS;
    }

    int mdb_txn_commit(MDB_txn* txn)
    {
        return end_txn(txn, true);
    }

    void mdb_txn_abort(MDB_txn* txn)
    {
        if (txn != nullptr)
            end_txn(txn, false);
    }

    int mdb_dbi_open(MDB_txn* txn, const char* name, unsigned int flags, MDB_dbi* dbi)
    {
        if (txn == nullptr || dbi == nullptr)
            return EINVAL;
        if (flags & (MDB_REVERSEKEY | MDB_INTEGERKEY | MDB_DUPFIXED | MDB_INTEGERDUP | MDB_REVERSEDUP))
            return set_error(EINVAL, "Database flags not supported");

        const std::string sname = name != nullptr ? name : "";
        const bool dupsort = (flags & MDB_DUPSORT) != 0;
        const std::string ckey = "d" + sname;

        std::string info;
        int rc = catalog_get(txn, ckey, &info);
        if (rc && rc != MDB_NOTFOUND)
            return rc;

        uint32_t root = 0;
        bool created = false;
        if (rc == MDB_SUCCESS)
        {
            if (info.size() != 8)
                return set_error(MDB_CORRUPTED, "Invalid catalog entry for database " + sname);
            root = get_u32(info.data());
            uint32_t stored_flags = get_u32(info.data() + 4);
            if (((stored_flags & MDB_DUPSORT) != 0) != dupsort)
                return MDB_INCOMPATIBLE;
        }
        else
        {
            if (!(flags & MDB_CREATE))
                return MDB_NOTFOUND;
            if (txn->rdonly)
                return set_error(EACCES, "Cannot create database in read-only transaction");

            modified(txn);
            int brc = lsbt_create_tree(txn->conn->c, &root);
            if (brc != LSBT_OK)
                return bt_error(brc);

            info.assign(8, '\0');
            put_u32(info.data(), root);
            put_u32(info.data() + 4, dupsort ? MDB_DUPSORT : 0);
            rc = catalog_put(txn, ckey, info);
            if (rc)
                return rc;
            created = true;
        }

        unsigned slot;
        DbiInfo* d = find_or_add_dbi(txn->env, sname, &slot);
        if (d == nullptr)
            return MDB_DBS_FULL;

        {
            std::scoped_lock lock(txn->env->mutex);
            if (!d->opened.load(std::memory_order_relaxed))
            {
                d->dupsort = dupsort;
                d->root.store(root, std::memory_order_relaxed);
                d->created_txnid.store(created ? txn->txnid : 0, std::memory_order_relaxed);
                d->opened.store(true, std::memory_order_release);
                if (created)
                    txn->created.push_back(d);
            }
        }

        *dbi = slot + dbi_offset;
        return MDB_SUCCESS;
    }

    int mdb_set_compare(MDB_txn* txn, MDB_dbi dbi, MDB_cmp_func* cmp)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr)
            return EINVAL;
        d->cmp.store(cmp, std::memory_order_relaxed);
        return MDB_SUCCESS;
    }

    int mdb_set_dupsort(MDB_txn* txn, MDB_dbi dbi, MDB_cmp_func* cmp)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr)
            return EINVAL;
        d->dcmp.store(cmp, std::memory_order_relaxed);
        return MDB_SUCCESS;
    }

    int mdb_get(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || key == nullptr || data == nullptr)
            return EINVAL;
        if (key->mv_size == 0)
            return MDB_BAD_VALSIZE;

        lsbt_cursor* bc;
        int rc = tmp_cursor(txn, slot, d, &bc);
        if (rc)
            return rc;
        rc = seek_exact(bc, d, key, nullptr);
        if (rc)
            return rc;
        lsbt_kv kv;
        int brc = lsbt_read(bc, &kv);
        if (brc != LSBT_OK)
            return bt_result(brc);
        assign(txn->get_buf, kv.v, kv.nv);
        data->mv_size = txn->get_buf.size();
        data->mv_data = txn->get_buf.empty() ? empty_buf : txn->get_buf.data();
        return MDB_SUCCESS;
    }

    int mdb_put(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data, unsigned int flags)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || key == nullptr || data == nullptr)
            return EINVAL;
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        lsbt_cursor* bc;
        int rc = tmp_cursor(txn, slot, d, &bc);
        if (rc)
            return rc;
        return do_put(txn, bc, d, key, data, flags);
    }

    int mdb_del(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || key == nullptr)
            return EINVAL;
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        if (key->mv_size == 0)
            return MDB_BAD_VALSIZE;

        lsbt_cursor* bc;
        int rc = tmp_cursor(txn, slot, d, &bc);
        if (rc)
            return rc;

        if (d->dupsort && data != nullptr)
        {
            rc = seek_exact(bc, d, key, data);
            if (rc)
                return rc;
            modified(txn);
            int brc = lsbt_delete(bc);
            return brc == LSBT_OK ? MDB_SUCCESS : bt_error(brc);
        }
        return delete_key(txn, bc, d, key);
    }

    int mdb_cursor_open(MDB_txn* txn, MDB_dbi dbi, MDB_cursor** cursor)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || cursor == nullptr)
            return EINVAL;

        auto c = std::make_unique<MDB_cursor>();
        c->txn = txn;
        c->d = d;
        c->slot = slot;
        int rc = open_bc(txn, slot, d, &c->bc);
        if (rc)
            return rc;
        txn->cursors.push_back(c.get());
        *cursor = c.release();
        return MDB_SUCCESS;
    }

    void mdb_cursor_close(MDB_cursor* c)
    {
        if (c == nullptr)
            return;

        MDB_txn* txn = c->txn;
        if (txn != nullptr)
        {
            c->close_bc();
            // Keep returned data valid until the end of the transaction
            for (Entry& e : c->ring)
            {
                if (!e.k.empty())
                    txn->retained.push_back(std::move(e.k));
                if (!e.v.empty())
                    txn->retained.push_back(std::move(e.v));
            }
            for (size_t i = 0; i < txn->cursors.size(); ++i)
            {
                if (txn->cursors[i] == c)
                {
                    txn->cursors[i] = txn->cursors.back();
                    txn->cursors.pop_back();
                    break;
                }
            }
        }
        delete c;
    }

    int mdb_cursor_get(MDB_cursor* c, MDB_val* key, MDB_val* data, MDB_cursor_op op)
    {
        if (c == nullptr || c->txn == nullptr)
            return EINVAL;

        int rc;
        bool set_key = true;
        switch (op)
        {
            case MDB_FIRST:
                rc = cursor_result(c, lsbt_first(c->bc));
                break;
            case MDB_LAST:
                rc = cursor_result(c, lsbt_last(c->bc));
                break;
            case MDB_NEXT:
                rc = cursor_move(c, 1);
                break;
            case MDB_PREV:
                rc = cursor_move(c, -1);
                break;
            case MDB_GET_CURRENT:
                if (!c->positioned)
                    return EINVAL;
                if (c->deleted)
                    return MDB_NOTFOUND;
                rc = MDB_SUCCESS;
                break;
            case MDB_SET:
            case MDB_SET_KEY:
            case MDB_SET_RANGE:
                if (key == nullptr)
                    return EINVAL;
                if (key->mv_size == 0)
                    return MDB_BAD_VALSIZE;
                set_key = op != MDB_SET;
                if (op == MDB_SET_RANGE)
                {
                    rc = cursor_result(c, lsbt_seek(c->bc, val_data(key), key->mv_size, nullptr, 0, LSBT_SEEK_GE, nullptr));
                }
                else
                {
                    rc = seek_exact(c->bc, c->d, key, nullptr);
                    if (!rc)
                        rc = store_current(c);
                }
                if (rc == MDB_NOTFOUND)
                    c->positioned = false;
                break;
            case MDB_GET_BOTH:
                if (key == nullptr || data == nullptr)
                    return EINVAL;
                if (key->mv_size == 0)
                    return MDB_BAD_VALSIZE;
                rc = seek_exact(c->bc, c->d, key, data);
                if (!rc)
                    rc = store_current(c);
                if (!rc && !c->d->dupsort && memn_cmp(c->current().v.data(), c->current().v.size(), val_data(data), data->mv_size) != 0)
                    rc = MDB_NOTFOUND;
                if (rc == MDB_NOTFOUND)
                    c->positioned = false;
                break;
            default:
                return set_error(EINVAL, "Cursor operation not supported");
        }

        if (rc)
            return rc;

        if (key != nullptr && set_key)
            set_val(key, c->current().k);
        if (data != nullptr)
            set_val(data, c->current().v);
        return MDB_SUCCESS;
    }

    int mdb_cursor_put(MDB_cursor* c, MDB_val* key, MDB_val* data, unsigned int flags)
    {
        if (c == nullptr || c->txn == nullptr || key == nullptr || data == nullptr)
            return EINVAL;

        MDB_txn* txn = c->txn;
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");

        if (flags & MDB_CURRENT)
        {
            if (!c->positioned || c->deleted)
                return EINVAL;

            Entry pos = c->current();
            int rc;
            if (c->d->dupsort)
            {
                // Data is part of the B-tree key: replace the pair
                rc = seek_entry(c->bc, c->d, pos);
                if (!rc)
                {
                    modified(txn);
                    int brc = lsbt_delete(c->bc);
                    if (brc != LSBT_OK)
                        rc = bt_error(brc);
                }
            }
            else
            {
                rc = MDB_SUCCESS;
                modified(txn);
            }
            if (rc)
                return rc;
            int brc = lsbt_put(c->bc, pos.k.empty() ? empty_buf : pos.k.data(), pos.k.size(), val_data(data), data->mv_size);
            if (brc != LSBT_OK)
                return bt_error(brc);
            c->store_kv(pos.k.empty() ? empty_buf : pos.k.data(), pos.k.size(), val_data(data), data->mv_size);
            return MDB_SUCCESS;
        }

        int rc = do_put(txn, c->bc, c->d, key, data, flags);
        if (rc == MDB_SUCCESS || (rc == MDB_KEYEXIST && (flags & MDB_NOOVERWRITE)))
            c->store_kv(val_data(key), key->mv_size, val_data(data), data->mv_size);
        return rc;
    }

    int mdb_cursor_del(MDB_cursor* c, unsigned int flags)
    {
        if (c == nullptr || c->txn == nullptr)
            return EINVAL;
        if (c->txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        if (!c->positioned || c->deleted)
            return EINVAL;

        Entry& pos = c->current();
        int rc;
        if (c->d->dupsort && (flags & MDB_NODUPDATA))
        {
            MDB_val k;
            set_val(&k, pos.k);
            rc = delete_key(c->txn, c->bc, c->d, &k);
        }
        else
        {
            rc = seek_entry(c->bc, c->d, pos);
            if (!rc)
            {
                modified(c->txn);
                int brc = lsbt_delete(c->bc);
                if (brc != LSBT_OK)
                    rc = bt_error(brc);
            }
        }
        if (rc)
            return rc;

        c->deleted = true;
        return MDB_SUCCESS;
    }

    int mdb_madvise(MDB_env* /*env*/, int /*random*/)
    {
        return 0;
    }

    void mdb_env_unmap(MDB_env* /*env*/)
    {
    }

    size_t mdb_get_txnid(MDB_txn* txn)
    {
        return txn != nullptr ? txn->txnid : 0;
    }

    void mdb_env_get_txnids(MDB_env* env, size_t* txnid1, size_t* txnid2)
    {
        size_t t = env != nullptr ? env->last_txnid.load() : 0;
        *txnid1 = t;
        *txnid2 = t > 0 ? t - 1 : 0;
    }

    void mdb_get_map(MDB_env* /*env*/, char** p_mmap, size_t* p_size)
    {
        *p_mmap = nullptr;
        *p_size = 0;
    }

    int mdb_cursor_first_leaf_page(MDB_cursor* c, size_t* pgno)
    {
        if (pgno == nullptr || c == nullptr || c->txn == nullptr)
            return MDB_INVALID;

        int rc = cursor_result(c, lsbt_first(c->bc));
        if (rc)
            return rc;
        return cursor_pgno(c, pgno);
    }

    int mdb_cursor_next_leaf_page(MDB_cursor* c, size_t* pgno)
    {
        if (pgno == nullptr || c == nullptr || c->txn == nullptr)
            return MDB_INVALID;
        if (!c->positioned)
            return EINVAL;

        int rc = cursor_result(c, lsbt_next_page(c->bc));
        if (rc)
            return rc;
        return cursor_pgno(c, pgno);
    }

    int mdb_cursor_get_pageno(MDB_cursor* c, size_t* pgno)
    {
        if (pgno == nullptr || c == nullptr || c->txn == nullptr)
            return MDB_INVALID;
        if (!c->positioned)
            return EINVAL;
        return cursor_pgno(c, pgno);
    }

    int mdb_page_get_nkeys(MDB_cursor* mc, size_t pgno, unsigned int* nkeys)
    {
        if (nkeys == nullptr || mc == nullptr)
            return MDB_INVALID;
        int rc = load_page(mc, pgno);
        if (rc)
            return rc;
        *nkeys = static_cast<unsigned int>(mc->page_size);
        return MDB_SUCCESS;
    }

    int mdb_page_get_val(MDB_cursor* mc, size_t pgno, unsigned int idx, MDB_val* key, MDB_val* data)
    {
        if (mc == nullptr || key == nullptr || data == nullptr)
            return MDB_INVALID;
        int rc = load_page(mc, pgno);
        if (rc)
            return rc;
        if (idx >= mc->page_size)
            return MDB_INVALID;
        set_val(key, mc->page[idx].k);
        set_val(data, mc->page[idx].v);
        return MDB_SUCCESS;
    }

    int mdb_page_is_dirty(MDB_cursor* mc, size_t /*pgno*/, int* dirty)
    {
        if (mc == nullptr || dirty == nullptr)
            return MDB_INVALID;
        if (mc->txn == nullptr)
            return MDB_PAGE_NOTFOUND;
        // Pages are updated in place (the WAL keeps the old versions), rewriting does not free anything
        *dirty = 1;
        return MDB_SUCCESS;
    }

} // extern "C"
