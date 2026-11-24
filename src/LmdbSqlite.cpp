/*
 * SQLite backed implementation of the LMDB API subset declared in LmdbSqlite.h.
 * See the header for the mapping and the differences to LMDB.
 */
#include "LmdbSqlite.h"
#include "ThreadSafety.h"

#include <sqlite-cpp-sqlgen/sqlite3.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr size_t max_dbis = 128;
    // dbi handles start at 2 (LMDB: 0 = free DB, 1 = main DB)
    constexpr unsigned dbi_offset = 2;
    constexpr size_t cursor_ring_size = 4;
    constexpr int64_t page_entries = LMDB_SQLITE_PAGE_ENTRIES;

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

    int sql_error(sqlite3* db, int rc)
    {
        int ret;
        switch (rc & 0xff)
        {
            case SQLITE_FULL:
                ret = ENOSPC;
                break;
            case SQLITE_IOERR:
                ret = EIO;
                break;
            case SQLITE_NOMEM:
                ret = ENOMEM;
                break;
            case SQLITE_CORRUPT:
            case SQLITE_NOTADB:
                ret = MDB_CORRUPTED;
                break;
            case SQLITE_READONLY:
            case SQLITE_PERM:
            case SQLITE_AUTH:
                ret = EACCES;
                break;
            case SQLITE_BUSY:
            case SQLITE_LOCKED:
                ret = EBUSY;
                break;
            case SQLITE_CANTOPEN:
                ret = ENOENT;
                break;
            case SQLITE_TOOBIG:
                ret = MDB_BAD_VALSIZE;
                break;
            default:
                ret = MDB_PANIC;
                break;
        }
        return set_error(ret, std::string("SQLite: ") + (db != nullptr ? sqlite3_errmsg(db) : sqlite3_errstr(rc)));
    }

    int memn_cmp(const void* a, size_t na, const void* b, size_t nb)
    {
        size_t len = na < nb ? na : nb;
        int diff = len > 0 ? memcmp(a, b, len) : 0;
        if (diff != 0)
            return diff;
        return na < nb ? -1 : (na > nb ? 1 : 0);
    }

    std::string quote_ident(const std::string& s)
    {
        std::string ret = "\"";
        for (char ch : s)
        {
            if (ch == '"')
                ret += "\"\"";
            else
                ret += ch;
        }
        ret += "\"";
        return ret;
    }

    const char* key_coll_prefix = "lmdbk_";
    const char* dup_coll_prefix = "lmdbd_";
    constexpr size_t coll_prefix_len = 6;

    enum Query
    {
        Q_GET,
        Q_PUT,
        Q_DEL_KEY,
        Q_DEL_PAIR,
        Q_FIRST,
        Q_LAST,
        Q_GT,
        Q_LT,
        Q_GE,
        Q_EQ,
        Q_BOTH,
        Q_UPDATE,
        Q_PAGE,
        Q_SKIP,
        Q_COUNT
    };

    struct DbiInfo
    {
        std::string name;
        std::atomic<MDB_cmp_func*> cmp{ nullptr };
        std::atomic<MDB_cmp_func*> dcmp{ nullptr };
        std::atomic<bool> opened{ false };
        // Set under MDB_env::mutex before opened is set, read only after opened
        bool dupsort = false;
        std::string sql[Q_COUNT];
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

    void build_sql(DbiInfo& d)
    {
        const std::string t = quote_ident("d_" + d.name);
        if (d.dupsort)
        {
            d.sql[Q_GET] = "SELECT v FROM " + t + " WHERE k = ?1 ORDER BY k, v LIMIT 1";
            d.sql[Q_PUT] = "INSERT OR IGNORE INTO " + t + " (k, v) VALUES (?1, ?2)";
            d.sql[Q_FIRST] = "SELECT k, v FROM " + t + " ORDER BY k, v";
            d.sql[Q_LAST] = "SELECT k, v FROM " + t + " ORDER BY k DESC, v DESC";
            d.sql[Q_GT] = "SELECT k, v FROM " + t + " WHERE (k, v) > (?1, ?2) ORDER BY k, v";
            d.sql[Q_LT] = "SELECT k, v FROM " + t + " WHERE (k, v) < (?1, ?2) ORDER BY k DESC, v DESC";
            d.sql[Q_GE] = "SELECT k, v FROM " + t + " WHERE k >= ?1 ORDER BY k, v";
            d.sql[Q_EQ] = "SELECT k, v FROM " + t + " WHERE k = ?1 ORDER BY k, v LIMIT 1";
            d.sql[Q_UPDATE] = "";
            d.sql[Q_PAGE] = "SELECT k, v FROM " + t + " WHERE (k, v) >= (?1, ?2) ORDER BY k, v LIMIT ?3";
            d.sql[Q_SKIP] = "SELECT k, v FROM " + t + " WHERE (k, v) >= (?1, ?2) ORDER BY k, v LIMIT 1 OFFSET ?3";
        }
        else
        {
            d.sql[Q_GET] = "SELECT v FROM " + t + " WHERE k = ?1";
            d.sql[Q_PUT] = "INSERT OR REPLACE INTO " + t + " (k, v) VALUES (?1, ?2)";
            d.sql[Q_FIRST] = "SELECT k, v FROM " + t + " ORDER BY k";
            d.sql[Q_LAST] = "SELECT k, v FROM " + t + " ORDER BY k DESC";
            d.sql[Q_GT] = "SELECT k, v FROM " + t + " WHERE k > ?1 ORDER BY k";
            d.sql[Q_LT] = "SELECT k, v FROM " + t + " WHERE k < ?1 ORDER BY k DESC";
            d.sql[Q_GE] = "SELECT k, v FROM " + t + " WHERE k >= ?1 ORDER BY k";
            d.sql[Q_EQ] = "SELECT k, v FROM " + t + " WHERE k = ?1";
            d.sql[Q_UPDATE] = "UPDATE " + t + " SET v = ?2 WHERE k = ?1";
            d.sql[Q_PAGE] = "SELECT k, v FROM " + t + " WHERE k >= ?1 ORDER BY k LIMIT ?3";
            d.sql[Q_SKIP] = "SELECT k, v FROM " + t + " WHERE k >= ?1 ORDER BY k LIMIT 1 OFFSET ?3";
        }
        d.sql[Q_DEL_KEY] = "DELETE FROM " + t + " WHERE k = ?1";
        d.sql[Q_DEL_PAIR] = "DELETE FROM " + t + " WHERE k = ?1 AND v = ?2";
        d.sql[Q_BOTH] = "SELECT k, v FROM " + t + " WHERE k = ?1 AND v = ?2";
    }

    std::string create_table_sql(const std::string& name, bool dupsort)
    {
        const std::string t = quote_ident("d_" + name);
        const std::string ck = quote_ident(key_coll_prefix + name);
        if (dupsort)
        {
            const std::string cd = quote_ident(dup_coll_prefix + name);
            return "CREATE TABLE " + t + " (k TEXT COLLATE " + ck + " NOT NULL, v TEXT COLLATE " + cd +
                   " NOT NULL, PRIMARY KEY (k, v)) WITHOUT ROWID";
        }
        return "CREATE TABLE " + t + " (k TEXT COLLATE " + ck + " NOT NULL PRIMARY KEY, v BLOB NOT NULL) WITHOUT ROWID";
    }

    struct Conn
    {
        sqlite3* db = nullptr;
        // Free prepared statements, indexed by dbi slot * Q_COUNT + query
        std::vector<std::vector<sqlite3_stmt*>> stmts;
        std::unordered_map<std::string, sqlite3_stmt*> misc;

        Conn() : stmts(max_dbis * Q_COUNT) {}

        ~Conn()
        {
            for (auto& v : stmts)
                for (sqlite3_stmt* s : v)
                    sqlite3_finalize(s);
            for (auto& it : misc)
                sqlite3_finalize(it.second);
            if (db != nullptr)
                sqlite3_close_v2(db);
        }

        Conn(const Conn&) = delete;
        Conn& operator=(const Conn&) = delete;
    };

    // Statement checked out of the per connection cache. Returned on destruction.
    struct Stmt
    {
        Conn* conn = nullptr;
        size_t idx = 0;
        sqlite3_stmt* s = nullptr;

        Stmt() = default;
        Stmt(const Stmt&) = delete;
        Stmt& operator=(const Stmt&) = delete;
        Stmt(Stmt&& o) noexcept : conn(o.conn), idx(o.idx), s(o.s) { o.s = nullptr; }
        Stmt& operator=(Stmt&& o) noexcept
        {
            if (this != &o)
            {
                release();
                conn = o.conn;
                idx = o.idx;
                s = o.s;
                o.s = nullptr;
            }
            return *this;
        }
        ~Stmt() { release(); }

        void release()
        {
            if (s != nullptr)
            {
                sqlite3_reset(s);
                sqlite3_clear_bindings(s);
                conn->stmts[idx].push_back(s);
                s = nullptr;
            }
        }
    };

    int checkout(Conn* conn, unsigned slot, const DbiInfo* d, Query q, Stmt& out)
    {
        size_t idx = slot * Q_COUNT + q;
        auto& v = conn->stmts[idx];
        sqlite3_stmt* s;
        if (!v.empty())
        {
            s = v.back();
            v.pop_back();
        }
        else
        {
            if (d->sql[q].empty())
                return set_error(EINVAL, "Operation not supported on this database");

            int rc = sqlite3_prepare_v3(conn->db, d->sql[q].c_str(), -1, SQLITE_PREPARE_PERSISTENT, &s, nullptr);
            if (rc != SQLITE_OK)
                return sql_error(conn->db, rc);
        }
        out.release();
        out.conn = conn;
        out.idx = idx;
        out.s = s;
        return MDB_SUCCESS;
    }

    int misc_stmt(Conn* conn, const char* sql, sqlite3_stmt** out)
    {
        auto it = conn->misc.find(sql);
        if (it != conn->misc.end())
        {
            *out = it->second;
            return MDB_SUCCESS;
        }
        sqlite3_stmt* s;
        int rc = sqlite3_prepare_v3(conn->db, sql, -1, SQLITE_PREPARE_PERSISTENT, &s, nullptr);
        if (rc != SQLITE_OK)
            return sql_error(conn->db, rc);
        conn->misc[sql] = s;
        *out = s;
        return MDB_SUCCESS;
    }

    // Run a statement without result rows
    int exec_misc(Conn* conn, const char* sql)
    {
        sqlite3_stmt* s;
        int rc = misc_stmt(conn, sql, &s);
        if (rc)
            return rc;
        int src;
        while ((src = sqlite3_step(s)) == SQLITE_ROW)
        {
        }
        int ret = src == SQLITE_DONE ? MDB_SUCCESS : sql_error(conn->db, src);
        sqlite3_reset(s);
        return ret;
    }

    int exec_sql(Conn* conn, const std::string& sql)
    {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(conn->db, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK)
        {
            int ret = sql_error(conn->db, rc);
            if (errmsg != nullptr)
            {
                tl_err_msg = std::string("SQLite: ") + errmsg;
                sqlite3_free(errmsg);
            }
            return ret;
        }
        return MDB_SUCCESS;
    }

    void bind_text(sqlite3_stmt* s, int idx, const MDB_val* val)
    {
        const char* p = val->mv_data != nullptr ? static_cast<const char*>(val->mv_data) : empty_buf;
        sqlite3_bind_text64(s, idx, p, val->mv_size, SQLITE_TRANSIENT, SQLITE_UTF8);
    }

    void bind_data(sqlite3_stmt* s, int idx, const MDB_val* val, bool dupsort)
    {
        if (dupsort)
        {
            bind_text(s, idx, val);
            return;
        }
        const void* p = val->mv_data != nullptr ? val->mv_data : empty_buf;
        sqlite3_bind_blob64(s, idx, p, val->mv_size, SQLITE_TRANSIENT);
    }

    void column_assign(sqlite3_stmt* s, int col, std::vector<char>& out)
    {
        const char* p = static_cast<const char*>(sqlite3_column_blob(s, col));
        int n = sqlite3_column_bytes(s, col);
        if (p == nullptr || n <= 0)
            out.clear();
        else
            out.assign(p, p + n);
    }

    void set_val(MDB_val* out, std::vector<char>& v)
    {
        out->mv_size = v.size();
        out->mv_data = v.empty() ? empty_buf : v.data();
    }

    struct Entry
    {
        std::vector<char> k;
        std::vector<char> v;
    };

    const char* sql_get_txnid = "SELECT txnid FROM __lmdb_meta WHERE id = 0";
    const char* sql_set_txnid = "UPDATE __lmdb_meta SET txnid = ?1 WHERE id = 0";
    const char* sql_get_dbi = "SELECT flags FROM __lmdb_dbis WHERE name = ?1";
    const char* sql_add_dbi = "INSERT INTO __lmdb_dbis (name, flags) VALUES (?1, ?2)";

} // namespace

struct MDB_env
{
    std::string path;
    std::string file_path;
    unsigned int flags = 0;
    bool opened = false;

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

    // Separately opened WAL file, used to fsync commits (connections run with
    // synchronous=NORMAL). Closing any descriptor of the file drops the POSIX locks
    // SQLite holds on it, so it is only closed after all connections.
    int wal_fd = -1;

    // WAL checkpoints run in a separate thread on their own connection (the writer has
    // auto-checkpointing disabled by its WAL hook). Held while a checkpoint runs.
    std::mutex ckpt_run_mutex;
    std::unique_ptr<Conn> ckpt_conn GUARDED_BY(ckpt_run_mutex);
    std::thread ckpt_thread;
    std::mutex ckpt_mutex;
    std::condition_variable ckpt_cond;
    bool ckpt_stop GUARDED_BY(ckpt_mutex) = false;
    bool ckpt_pending GUARDED_BY(ckpt_mutex) = false;
    // Only used in wal_hook() on the writer connection
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
    // Closes all connections first, then the separately opened files (see MDB_env::wal_fd)
    // Checkpoint after this many new WAL frames (pages) since the last trigger
    constexpr int ckpt_frames = 4096;

    int wal_hook(void* arg, sqlite3* /*db*/, const char* /*db_name*/, int frames)
    {
        MDB_env* env = static_cast<MDB_env*>(arg);
        env->commits.fetch_add(1, std::memory_order_relaxed);
        // Only called on the writer connection, so serialized
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
        return SQLITE_OK;
    }

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
                int rc = sqlite3_wal_checkpoint_v2(env->ckpt_conn->db, nullptr, SQLITE_CHECKPOINT_PASSIVE, &n_log, &n_ckpt);
                complete = rc == SQLITE_OK && n_ckpt >= n_log;
            }
            done_commits = commits;
            lock.lock();
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

    void close_env_files(MDB_env* env) EXCLUDES(env->mutex, env->ckpt_mutex, env->ckpt_run_mutex)
    {
        stop_ckpt_thread(env);
        if (env->writer)
            sqlite3_wal_hook(env->writer->db, nullptr, nullptr);
        {
            std::scoped_lock lock(env->mutex);
            env->free_readers.clear();
        }
        {
            std::scoped_lock lock(env->ckpt_run_mutex);
            env->ckpt_conn.reset();
        }
        env->writer.reset();
        if (env->wal_fd != -1)
            close(env->wal_fd);
        env->wal_fd = -1;
    }
} // namespace

struct MDB_txn
{
    MDB_env* env = nullptr;
    Conn* conn = nullptr;
    std::unique_ptr<Conn> reader;
    bool rdonly = false;
    bool dirty = false;
    size_t txnid = 0;
    std::vector<MDB_cursor*> cursors;
    // Result of the last mdb_get() (or of an MDB_NOOVERWRITE put that found the key)
    std::vector<char> get_buf;
    std::vector<std::vector<char>> retained;
    std::vector<std::unique_ptr<std::vector<Entry>>> pages;

    std::vector<Entry>& get_pages(unsigned slot)
    {
        if (pages.empty())
            pages.resize(max_dbis);
        if (!pages[slot])
            pages[slot] = std::make_unique<std::vector<Entry>>();
        return *pages[slot];
    }
};

struct MDB_cursor
{
    MDB_txn* txn = nullptr;
    DbiInfo* d = nullptr;
    unsigned slot = 0;

    Entry ring[cursor_ring_size];
    size_t cur = 0;
    size_t next = 0;
    bool positioned = false;
    bool deleted = false;

    // Statement continuing the scan from the current position (only valid while
    // the dbi is not modified in this txn)
    Stmt live;
    int live_dir = 0;

    bool leaf_valid = false;
    size_t leaf = 0;

    size_t page_no = SIZE_MAX;
    std::vector<Entry> page;

    Entry& current() { return ring[cur]; }

    Entry& new_slot()
    {
        cur = next;
        next = (next + 1) % cursor_ring_size;
        positioned = true;
        deleted = false;
        return ring[cur];
    }

    void store_row(sqlite3_stmt* s)
    {
        Entry& e = new_slot();
        column_assign(s, 0, e.k);
        column_assign(s, 1, e.v);
    }

    void store_kv(const MDB_val* key, const MDB_val* data)
    {
        // Input may point into one of the ring entries
        const char* kp = static_cast<const char*>(key->mv_data);
        const char* dp = static_cast<const char*>(data->mv_data);
        std::vector<char> k(kp, kp + key->mv_size);
        std::vector<char> v(dp, dp + data->mv_size);
        Entry& e = new_slot();
        e.k.swap(k);
        e.v.swap(v);
    }

    void release_live()
    {
        live.release();
        live_dir = 0;
    }
};

namespace
{
    int open_conn(MDB_env* env, bool create, std::unique_ptr<Conn>& out);

    void collation_needed(void* arg, sqlite3* db, int /*rep*/, const char* name);

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
                if (slot_out != nullptr)
                    *slot_out = i;
                return d;
            }
            if (d->name == name)
            {
                if (slot_out != nullptr)
                    *slot_out = i;
                return d;
            }
        }
        return nullptr;
    }

    void collation_needed(void* arg, sqlite3* db, int /*rep*/, const char* name)
    {
        MDB_env* env = static_cast<MDB_env*>(arg);
        bool is_key = strncmp(name, key_coll_prefix, coll_prefix_len) == 0;
        bool is_dup = strncmp(name, dup_coll_prefix, coll_prefix_len) == 0;
        if (!is_key && !is_dup)
            return;

        DbiInfo* d = find_or_add_dbi(env, name + coll_prefix_len, nullptr);
        if (d == nullptr)
            return;

        sqlite3_create_collation_v2(db, name, SQLITE_UTF8, d, is_key ? key_collation : dup_collation, nullptr);
    }

    int open_conn(MDB_env* env, bool create, std::unique_ptr<Conn>& out)
    {
        auto conn = std::make_unique<Conn>();
        int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX | (create ? SQLITE_OPEN_CREATE : 0);
        int rc = sqlite3_open_v2(env->file_path.c_str(), &conn->db, flags, nullptr);
        if (rc != SQLITE_OK)
        {
            int ret = sql_error(conn->db, rc);
            tl_err_msg += " (" + env->file_path + ")";
            return ret;
        }

        sqlite3_busy_timeout(conn->db, 10 * 60 * 1000);
        sqlite3_collation_needed(conn->db, env, collation_needed);

        // Durability without MDB_NOSYNC comes from fsyncing the WAL in end_txn()
        rc = exec_sql(conn.get(), "PRAGMA synchronous=NORMAL;");
        if (rc)
        {
            if ((sqlite3_errcode(conn->db) & 0xff) == SQLITE_NOTADB)
                rc = set_error(MDB_INVALID, env->file_path + " is not an SQLite database (LMDB database?)");
            return rc;
        }

        out = std::move(conn);
        return MDB_SUCCESS;
    }

    int read_txnid(Conn* conn, size_t* txnid)
    {
        sqlite3_stmt* s;
        int rc = misc_stmt(conn, sql_get_txnid, &s);
        if (rc)
            return rc;
        int src = sqlite3_step(s);
        if (src == SQLITE_ROW)
        {
            *txnid = static_cast<size_t>(sqlite3_column_int64(s, 0));
            rc = MDB_SUCCESS;
        }
        else if (src == SQLITE_DONE)
        {
            rc = set_error(MDB_CORRUPTED, "LMDB-SQLite meta data missing");
        }
        else
        {
            rc = sql_error(conn->db, src);
        }
        sqlite3_reset(s);
        return rc;
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

    void invalidate_live(MDB_txn* txn, unsigned slot)
    {
        txn->dirty = true;
        for (MDB_cursor* c : txn->cursors)
        {
            if (c->slot == slot)
            {
                c->release_live();
                c->page_no = SIZE_MAX;
            }
        }
    }

    // Execute a modifying statement. Returns number of changed rows in *changes
    int exec_modify(MDB_txn* txn, unsigned slot, DbiInfo* d, Query q, const MDB_val* key, const MDB_val* data, int* changes)
    {
        Stmt st;
        int rc = checkout(txn->conn, slot, d, q, st);
        if (rc)
            return rc;

        invalidate_live(txn, slot);

        bind_text(st.s, 1, key);
        if (data != nullptr)
            bind_data(st.s, 2, data, d->dupsort);

        int src = sqlite3_step(st.s);
        if (src != SQLITE_DONE)
            return sql_error(txn->conn->db, src);

        if (changes != nullptr)
            *changes = sqlite3_changes(txn->conn->db);

        return MDB_SUCCESS;
    }

    // Single row lookup. Copies result column col into the txn get buffer (valid until the next lookup).
    int lookup(MDB_txn* txn, unsigned slot, DbiInfo* d, Query q, const MDB_val* key, const MDB_val* data, int col, MDB_val* out)
    {
        Stmt st;
        int rc = checkout(txn->conn, slot, d, q, st);
        if (rc)
            return rc;

        bind_text(st.s, 1, key);
        if (data != nullptr)
            bind_data(st.s, 2, data, d->dupsort);

        int src = sqlite3_step(st.s);
        if (src == SQLITE_DONE)
            return MDB_NOTFOUND;
        if (src != SQLITE_ROW)
            return sql_error(txn->conn->db, src);

        if (out != nullptr)
        {
            const void* p = sqlite3_column_blob(st.s, col);
            int n = sqlite3_column_bytes(st.s, col);
            out->mv_size = n > 0 ? static_cast<size_t>(n) : 0;
            if (p != nullptr && n > 0)
            {
                txn->get_buf.assign(static_cast<const char*>(p), static_cast<const char*>(p) + n);
                out->mv_data = txn->get_buf.data();
            }
            else
            {
                out->mv_data = empty_buf;
            }
        }
        return MDB_SUCCESS;
    }

    int do_put(MDB_txn* txn, unsigned slot, DbiInfo* d, MDB_val* key, MDB_val* data, unsigned int flags)
    {
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        if (key->mv_size == 0)
            return MDB_BAD_VALSIZE;
        if (flags & (MDB_RESERVE | MDB_MULTIPLE))
            return set_error(EINVAL, "MDB_RESERVE/MDB_MULTIPLE not supported");

        if (flags & MDB_NOOVERWRITE)
        {
            MDB_val existing;
            int rc = lookup(txn, slot, d, Q_GET, key, nullptr, 0, &existing);
            if (rc == MDB_SUCCESS)
            {
                *data = existing;
                return MDB_KEYEXIST;
            }
            if (rc != MDB_NOTFOUND)
                return rc;
        }
        else if (d->dupsort && (flags & MDB_NODUPDATA))
        {
            int rc = lookup(txn, slot, d, Q_BOTH, key, data, 1, nullptr);
            if (rc == MDB_SUCCESS)
                return MDB_KEYEXIST;
            if (rc != MDB_NOTFOUND)
                return rc;
        }

        return exec_modify(txn, slot, d, Q_PUT, key, data, nullptr);
    }

    void detach_cursor(MDB_cursor* c)
    {
        c->release_live();
        c->txn = nullptr;
    }

    int end_txn(MDB_txn* txn, bool commit)
    {
        if (txn == nullptr)
            return EINVAL;

        for (MDB_cursor* c : txn->cursors)
        {
            detach_cursor(c);
            // LMDB frees cursors of write transactions at transaction end
            if (!txn->rdonly)
                delete c;
        }
        txn->cursors.clear();

        MDB_env* env = txn->env;
        int ret = MDB_SUCCESS;

        if (!txn->rdonly)
        {
            if (commit)
            {
                if (txn->dirty)
                {
                    sqlite3_stmt* s;
                    ret = misc_stmt(txn->conn, sql_set_txnid, &s);
                    if (!ret)
                    {
                        sqlite3_bind_int64(s, 1, static_cast<sqlite3_int64>(txn->txnid));
                        int src = sqlite3_step(s);
                        if (src != SQLITE_DONE)
                            ret = sql_error(txn->conn->db, src);
                        sqlite3_reset(s);
                    }
                }
                if (!ret)
                    ret = exec_misc(txn->conn, "COMMIT");
                if (ret)
                {
                    std::string msg = tl_err_msg;
                    exec_misc(txn->conn, "ROLLBACK");
                    set_error(ret, msg);
                }
                else if (txn->dirty)
                {
                    env->last_txnid.store(txn->txnid);
                    if (!(env->flags & MDB_NOSYNC) && fsync(env->wal_fd) != 0)
                        ret = set_error(errno, "fsync of " + env->file_path + "-wal failed");
                }
            }
            else
            {
                exec_misc(txn->conn, "ROLLBACK");
            }

            {
                std::scoped_lock lock(env->write_mutex);
                env->write_busy = false;
            }
            env->write_cond.notify_one();
        }
        else
        {
            int rc = exec_misc(txn->conn, "COMMIT");
            if (rc)
            {
                exec_misc(txn->conn, "ROLLBACK");
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

    int cursor_query(MDB_cursor* c, Query q, int dir, const MDB_val* key, const MDB_val* data, int64_t limit_offset = -1)
    {
        c->release_live();

        Stmt st;
        int rc = checkout(c->txn->conn, c->slot, c->d, q, st);
        if (rc)
            return rc;

        if (key != nullptr)
            bind_text(st.s, 1, key);
        if (data != nullptr && sqlite3_bind_parameter_count(st.s) >= 2)
            bind_data(st.s, 2, data, c->d->dupsort);
        if (limit_offset >= 0)
            sqlite3_bind_int64(st.s, 3, limit_offset);

        int src = sqlite3_step(st.s);
        if (src == SQLITE_DONE)
            return MDB_NOTFOUND;
        if (src != SQLITE_ROW)
            return sql_error(c->txn->conn->db, src);

        c->store_row(st.s);

        if (dir != 0)
        {
            c->live = std::move(st);
            c->live_dir = dir;
        }
        return MDB_SUCCESS;
    }

    int cursor_step_live(MDB_cursor* c)
    {
        int src = sqlite3_step(c->live.s);
        if (src == SQLITE_ROW)
        {
            c->store_row(c->live.s);
            return MDB_SUCCESS;
        }
        c->release_live();
        if (src == SQLITE_DONE)
            return MDB_NOTFOUND;
        return sql_error(c->txn->conn->db, src);
    }

    int cursor_move(MDB_cursor* c, int dir)
    {
        if (!c->positioned)
            return cursor_query(c, dir > 0 ? Q_FIRST : Q_LAST, dir, nullptr, nullptr);

        if (c->live.s != nullptr && c->live_dir == dir)
            return cursor_step_live(c);

        // Copy, the ring entry may be overwritten by the result
        Entry pos = c->current();
        MDB_val k, v;
        set_val(&k, pos.k);
        set_val(&v, pos.v);
        return cursor_query(c, dir > 0 ? Q_GT : Q_LT, dir, &k, &v);
    }

    void pos_vals(MDB_cursor* c, MDB_val* k, MDB_val* v)
    {
        set_val(k, c->current().k);
        set_val(v, c->current().v);
    }

    int load_page(MDB_cursor* c, size_t pgno)
    {
        if (c->txn == nullptr)
            return EINVAL;

        std::vector<Entry>& pages = c->txn->get_pages(c->slot);
        if (pgno >= pages.size())
            return MDB_PAGE_NOTFOUND;

        if (c->page_no == pgno)
            return MDB_SUCCESS;

        Stmt st;
        int rc = checkout(c->txn->conn, c->slot, c->d, Q_PAGE, st);
        if (rc)
            return rc;

        MDB_val k, v;
        set_val(&k, pages[pgno].k);
        set_val(&v, pages[pgno].v);
        bind_text(st.s, 1, &k);
        if (sqlite3_bind_parameter_count(st.s) >= 2)
            bind_data(st.s, 2, &v, c->d->dupsort);
        sqlite3_bind_int64(st.s, 3, page_entries);

        size_t n = 0;
        int src;
        while ((src = sqlite3_step(st.s)) == SQLITE_ROW)
        {
            if (c->page.size() <= n)
                c->page.emplace_back();
            column_assign(st.s, 0, c->page[n].k);
            column_assign(st.s, 1, c->page[n].v);
            ++n;
        }
        if (src != SQLITE_DONE)
        {
            c->page_no = SIZE_MAX;
            return sql_error(c->txn->conn->db, src);
        }
        c->page.resize(n);
        c->page_no = pgno;
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

        // Not opening the file directly to check its header: closing that descriptor
        // would drop the POSIX locks of other connections to the file in this process.
        rc = exec_sql(env->writer.get(),
                      "PRAGMA journal_mode=WAL;"
                      "PRAGMA cache_size=-65536;"
                      "BEGIN IMMEDIATE;"
                      "CREATE TABLE IF NOT EXISTS __lmdb_meta (id INTEGER PRIMARY KEY, txnid INTEGER NOT NULL);"
                      "INSERT OR IGNORE INTO __lmdb_meta (id, txnid) VALUES (0, 0);"
                      "CREATE TABLE IF NOT EXISTS __lmdb_dbis (name TEXT PRIMARY KEY, flags INTEGER NOT NULL) WITHOUT ROWID;"
                      "COMMIT;");
        if (rc)
        {
            env->writer.reset();
            return rc;
        }

        size_t txnid;
        rc = read_txnid(env->writer.get(), &txnid);
        if (rc)
        {
            env->writer.reset();
            return rc;
        }

        // The WAL exists from the setup transaction above until the last connection closes
        env->wal_fd = open((env->file_path + "-wal").c_str(), O_RDONLY | O_CLOEXEC);
        if (env->wal_fd == -1)
        {
            rc = set_error(errno, "Opening " + env->file_path + "-wal for fsync failed");
            close_env_files(env);
            return rc;
        }

        std::unique_ptr<Conn> ckpt_conn;
        rc = open_conn(env, false, ckpt_conn);
        if (rc)
        {
            close_env_files(env);
            return rc;
        }
        {
            std::scoped_lock lock(env->ckpt_run_mutex);
            env->ckpt_conn = std::move(ckpt_conn);
        }
        sqlite3_wal_hook(env->writer->db, wal_hook, env);
        env->ckpt_thread = std::thread(ckpt_loop, env);

        env->last_txnid.store(txnid);
        env->opened = true;
        return MDB_SUCCESS;
    }

    void mdb_env_close(MDB_env* env)
    {
        if (env == nullptr)
            return;
        close_env_files(env);
        delete env;
    }

    int mdb_env_sync(MDB_env* env, int force)
    {
        if (env == nullptr || !env->opened)
            return EINVAL;

        if (!force && (env->flags & MDB_NOSYNC))
            return MDB_SUCCESS;

        // Committed data is in the WAL until a checkpoint copied it into the database file.
        // With synchronous=NORMAL a checkpoint fsyncs the database file before marking the
        // frames as backfilled, and only then may the writer restart (overwrite) the WAL.
        // So fsyncing the WAL, without a checkpoint running concurrently, makes all
        // commits durable.
        std::scoped_lock lock(env->ckpt_run_mutex);
        if (fsync(env->wal_fd) != 0)
            return set_error(errno, "fsync of " + env->file_path + "-wal failed");
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

            int rc = exec_misc(txn->conn, "BEGIN");
            if (!rc)
            {
                // Starts the read snapshot
                rc = read_txnid(txn->conn, &txn->txnid);
                if (rc)
                    exec_misc(txn->conn, "ROLLBACK");
            }
            if (rc)
                return rc;
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

            int rc = exec_misc(txn->conn, "BEGIN IMMEDIATE");
            size_t txnid = 0;
            if (!rc)
            {
                rc = read_txnid(txn->conn, &txnid);
                if (rc)
                    exec_misc(txn->conn, "ROLLBACK");
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

        sqlite3_stmt* s;
        int rc = misc_stmt(txn->conn, sql_get_dbi, &s);
        if (rc)
            return rc;
        sqlite3_bind_text(s, 1, sname.c_str(), static_cast<int>(sname.size()), SQLITE_TRANSIENT);
        int src = sqlite3_step(s);
        bool exists = false;
        int64_t stored_flags = 0;
        if (src == SQLITE_ROW)
        {
            exists = true;
            stored_flags = sqlite3_column_int64(s, 0);
        }
        else if (src != SQLITE_DONE)
        {
            rc = sql_error(txn->conn->db, src);
        }
        sqlite3_reset(s);
        sqlite3_clear_bindings(s);
        if (rc)
            return rc;

        if (exists)
        {
            if (((stored_flags & MDB_DUPSORT) != 0) != dupsort)
                return MDB_INCOMPATIBLE;
        }
        else
        {
            if (!(flags & MDB_CREATE))
                return MDB_NOTFOUND;
            if (txn->rdonly)
                return set_error(EACCES, "Cannot create database in read-only transaction");

            rc = exec_sql(txn->conn, create_table_sql(sname, dupsort));
            if (rc)
                return rc;

            rc = misc_stmt(txn->conn, sql_add_dbi, &s);
            if (rc)
                return rc;
            sqlite3_bind_text(s, 1, sname.c_str(), static_cast<int>(sname.size()), SQLITE_TRANSIENT);
            sqlite3_bind_int64(s, 2, dupsort ? MDB_DUPSORT : 0);
            src = sqlite3_step(s);
            sqlite3_reset(s);
            sqlite3_clear_bindings(s);
            if (src != SQLITE_DONE)
                return sql_error(txn->conn->db, src);
            txn->dirty = true;
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
                build_sql(*d);
                d->opened.store(true, std::memory_order_release);
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
        return lookup(txn, slot, d, Q_GET, key, nullptr, 0, data);
    }

    int mdb_put(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data, unsigned int flags)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || key == nullptr || data == nullptr)
            return EINVAL;
        return do_put(txn, slot, d, key, data, flags);
    }

    int mdb_del(MDB_txn* txn, MDB_dbi dbi, MDB_val* key, MDB_val* data)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || key == nullptr)
            return EINVAL;
        if (txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");

        int changes = 0;
        int rc;
        if (d->dupsort && data != nullptr)
            rc = exec_modify(txn, slot, d, Q_DEL_PAIR, key, data, &changes);
        else
            rc = exec_modify(txn, slot, d, Q_DEL_KEY, key, nullptr, &changes);
        if (rc)
            return rc;
        return changes > 0 ? MDB_SUCCESS : MDB_NOTFOUND;
    }

    int mdb_cursor_open(MDB_txn* txn, MDB_dbi dbi, MDB_cursor** cursor)
    {
        unsigned slot;
        DbiInfo* d = get_dbi(txn, dbi, &slot);
        if (d == nullptr || cursor == nullptr)
            return EINVAL;

        MDB_cursor* c = new MDB_cursor;
        c->txn = txn;
        c->d = d;
        c->slot = slot;
        txn->cursors.push_back(c);
        *cursor = c;
        return MDB_SUCCESS;
    }

    void mdb_cursor_close(MDB_cursor* c)
    {
        if (c == nullptr)
            return;

        MDB_txn* txn = c->txn;
        if (txn != nullptr)
        {
            c->release_live();
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

        c->leaf_valid = false;

        int rc;
        bool set_key = true;
        switch (op)
        {
            case MDB_FIRST:
                rc = cursor_query(c, Q_FIRST, 1, nullptr, nullptr);
                break;
            case MDB_LAST:
                rc = cursor_query(c, Q_LAST, -1, nullptr, nullptr);
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
                    rc = cursor_query(c, Q_GE, 1, key, nullptr);
                else
                    rc = cursor_query(c, Q_EQ, 0, key, nullptr);
                if (rc == MDB_NOTFOUND)
                    c->positioned = false;
                break;
            case MDB_GET_BOTH:
                if (key == nullptr || data == nullptr)
                    return EINVAL;
                if (key->mv_size == 0)
                    return MDB_BAD_VALSIZE;
                rc = cursor_query(c, Q_BOTH, 0, key, data);
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

        c->leaf_valid = false;

        if (flags & MDB_CURRENT)
        {
            if (!c->positioned || c->deleted)
                return EINVAL;

            Entry pos = c->current();
            MDB_val k, v;
            set_val(&k, pos.k);
            set_val(&v, pos.v);

            int rc;
            if (c->d->dupsort)
            {
                rc = exec_modify(txn, c->slot, c->d, Q_DEL_PAIR, &k, &v, nullptr);
                if (!rc)
                    rc = exec_modify(txn, c->slot, c->d, Q_PUT, &k, data, nullptr);
            }
            else
            {
                rc = exec_modify(txn, c->slot, c->d, Q_UPDATE, &k, data, nullptr);
            }
            if (rc)
                return rc;
            c->store_kv(&k, data);
            return MDB_SUCCESS;
        }

        int rc = do_put(txn, c->slot, c->d, key, data, flags);
        if (rc == MDB_KEYEXIST && (flags & MDB_NOOVERWRITE))
        {
            c->store_kv(key, data);
            return rc;
        }
        if (rc)
            return rc;
        c->store_kv(key, data);
        return MDB_SUCCESS;
    }

    int mdb_cursor_del(MDB_cursor* c, unsigned int flags)
    {
        if (c == nullptr || c->txn == nullptr)
            return EINVAL;
        if (c->txn->rdonly)
            return set_error(EACCES, "Write in read-only transaction");
        if (!c->positioned || c->deleted)
            return EINVAL;

        c->leaf_valid = false;

        MDB_val k, v;
        pos_vals(c, &k, &v);
        int rc;
        if (c->d->dupsort && !(flags & MDB_NODUPDATA))
            rc = exec_modify(c->txn, c->slot, c->d, Q_DEL_PAIR, &k, &v, nullptr);
        else
            rc = exec_modify(c->txn, c->slot, c->d, Q_DEL_KEY, &k, nullptr, nullptr);
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

        int rc = cursor_query(c, Q_FIRST, 0, nullptr, nullptr);
        if (rc)
            return rc;

        std::vector<Entry>& pages = c->txn->get_pages(c->slot);
        pages.clear();
        pages.push_back(c->current());
        for (MDB_cursor* oc : c->txn->cursors)
            if (oc->slot == c->slot)
                oc->page_no = SIZE_MAX;

        c->leaf_valid = true;
        c->leaf = 0;
        *pgno = 0;
        return MDB_SUCCESS;
    }

    int mdb_cursor_next_leaf_page(MDB_cursor* c, size_t* pgno)
    {
        if (pgno == nullptr || c == nullptr || c->txn == nullptr)
            return MDB_INVALID;
        if (!c->positioned)
            return EINVAL;

        std::vector<Entry>& pages = c->txn->get_pages(c->slot);

        if (c->leaf_valid && c->leaf < pages.size())
        {
            if (c->leaf + 1 < pages.size())
            {
                Entry e = pages[c->leaf + 1];
                c->release_live();
                Entry& ne = c->new_slot();
                ne.k.swap(e.k);
                ne.v.swap(e.v);
            }
            else
            {
                MDB_val k, v;
                set_val(&k, pages[c->leaf].k);
                set_val(&v, pages[c->leaf].v);
                int rc = cursor_query(c, Q_SKIP, 0, &k, &v, page_entries);
                if (rc)
                    return rc;
                pages.push_back(c->current());
            }
            ++c->leaf;
        }
        else
        {
            // Cursor not positioned via leaf pages. Next page starts after the current item.
            int rc = cursor_move(c, 1);
            c->release_live();
            if (rc)
                return rc;
            pages.push_back(c->current());
            c->leaf = pages.size() - 1;
        }

        c->leaf_valid = true;
        *pgno = c->leaf;
        return MDB_SUCCESS;
    }

    int mdb_cursor_get_pageno(MDB_cursor* c, size_t* pgno)
    {
        if (pgno == nullptr || c == nullptr || c->txn == nullptr)
            return MDB_INVALID;
        if (!c->leaf_valid)
        {
            if (!c->positioned)
                return EINVAL;
            std::vector<Entry>& pages = c->txn->get_pages(c->slot);
            pages.push_back(c->current());
            c->leaf = pages.size() - 1;
            c->leaf_valid = true;
        }
        *pgno = c->leaf;
        return MDB_SUCCESS;
    }

    int mdb_page_get_nkeys(MDB_cursor* mc, size_t pgno, unsigned int* nkeys)
    {
        if (nkeys == nullptr || mc == nullptr)
            return MDB_INVALID;
        int rc = load_page(mc, pgno);
        if (rc)
            return rc;
        *nkeys = static_cast<unsigned int>(mc->page.size());
        return MDB_SUCCESS;
    }

    int mdb_page_get_val(MDB_cursor* mc, size_t pgno, unsigned int idx, MDB_val* key, MDB_val* data)
    {
        if (mc == nullptr || key == nullptr || data == nullptr)
            return MDB_INVALID;
        int rc = load_page(mc, pgno);
        if (rc)
            return rc;
        if (idx >= mc->page.size())
            return MDB_INVALID;
        set_val(key, mc->page[idx].k);
        set_val(data, mc->page[idx].v);
        return MDB_SUCCESS;
    }

    int mdb_page_is_dirty(MDB_cursor* mc, size_t pgno, int* dirty)
    {
        if (mc == nullptr || dirty == nullptr)
            return MDB_INVALID;
        if (mc->txn == nullptr || pgno >= mc->txn->get_pages(mc->slot).size())
            return MDB_PAGE_NOTFOUND;
        // No pages to rewrite in SQLite
        *dirty = 1;
        return MDB_SUCCESS;
    }

} // extern "C"
