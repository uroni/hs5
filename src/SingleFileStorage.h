#pragma once
#include "../external/lmdb/lmdb.h"
#include "File.h"
#include "crypt.h"
#include "relaxed_atomic.h"
#include <array>
#include <assert.h>
#include <condition_variable>
#include <filesystem>
#include <folly/io/IOBuf.h>
#include <folly/io/IOBufQueue.h>
#include <functional>
#include <iostream>
#include <map>
#include <memory.h>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <quic/common/Expected.h>
#include <set>
#include <shared_mutex>
#include <sodium.h>
#include <stdint.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using THREAD_ID = pid_t;

constexpr int64_t sfs_block_size = 4096;

class WalFile;

class SingleFileStorage
{
    friend WalFile;

public:
    struct SPunchItem
    {
        SPunchItem()
            : offset(-1), len(0) {}

        SPunchItem(int64_t offset, int64_t len)
            : offset(offset), len(len) {}

        bool operator<(const SPunchItem& other) const
        {
            return offset < other.offset;
        }

        int64_t offset;
        int64_t len;
    };

    struct Ext
    {
        Ext()
            : obj_offset(0), data_file_offset(-1), len(0) {}

        Ext(int64_t obj_offset, int64_t data_file_offset, int64_t len)
            : obj_offset(obj_offset), data_file_offset(data_file_offset), len(len) {}

        bool operator<(const Ext& other) const
        {
            return obj_offset < other.obj_offset;
        }

        int64_t obj_offset;
        int64_t data_file_offset;
        int64_t len;
    };

    enum class DelAction
    {
        Del = 0,
        DelOld = 1,
        DelWithQueued = 2,
        Queue = 3,
        Unqueue = 4,
        AssertQueueEmpty = 5,
        DelNoCallback = 6
    };

    enum class FragAction
    {
        Add,
        Del,
        DelNoCallback,
        Commit,
        FindFree,
        AddNoDelOld,
        DelOld,
        RestoreOld,
        EmptyQueue,
        ReadFragInfo,
        ReadFragInfoWithoutParsing,
        FreeExtents,
        ResetDelLog,
        GetDiskId,
        QueueDel,
        UnqueueDel,
        DelWithQueued,
        ResetDelQueue,
        AssertDelQueueEmpty,
        DelFreemapWal,
        AddFreemapWal,
        ModifyData,
        ModifyMetadata
    };

    struct SFragInfo;

    struct MatchInfo
    {
        std::optional<std::string> readFn;
        std::vector<std::string> ifMatch;
        std::vector<std::string> ifNoneMatch;
        folly::Optional<int64_t> ifModifiedSince;
        folly::Optional<int64_t> ifUnmodifiedSince;
        // Match only if the current object has this version (requires readFn with readNewest). Delete markers are
        // compared as well. Not found matches (if ifMatch is empty)
        std::optional<int64_t> ifVersion;
        // Match only if the current object has this content type (ContentType::contentType, 0 for none/unlisted).
        // Delete markers/not found match (if ifMatch is empty)
        std::optional<int> ifContentType;
        // Time (seconds since epoch) at which protection is checked (with checkProtected). Current time if 0
        int64_t protectedCheckTime = 0;
        bool readNewest : 1 = false;
        bool match : 1 = false;
        // Set if match is false because of If-None-Match/If-Modified-Since (304 Not Modified for GET/HEAD)
        bool notModified : 1 = false;
        // If set, the match callback sets match=false and isProtected=true if the current object is protected from modification
        bool checkProtected : 1 = false;
        bool isProtected : 1 = false;
        // Governance mode retention does not protect the object (with checkProtected)
        bool bypassGovernance : 1 = false;
        // Set if match is false because of If-Match and there is no current object (404 Not Found for writes)
        bool notFound : 1 = false;
        // Set by the match callback if the checked object is a delete marker
        bool isDeleteMarker : 1 = false;

        void setNotFound()
        {
            match = ifMatch.empty();
            notFound = !match;
            notModified = false;
            isDeleteMarker = false;
        }
    };

    struct SCommitInfo
    {
        SCommitInfo()
            : commit_errors(0),
              frag_info(nullptr), match_info(nullptr)
        {
        }
        int64_t commit_errors;
        std::mutex commit_done_mutex;
        std::condition_variable commit_done;
        int64_t new_datafile_offset;
        int64_t new_datafile_offset_end;
        SFragInfo* frag_info;
        MatchInfo* match_info;
    };

    struct SFragInfo
    {
        SFragInfo() : offset(-1), len(0),
                      last_modified(0), commit_info(nullptr), linked(nullptr)
        {
        }
        SFragInfo(int64_t offset, int64_t len)
            : offset(offset), len(len),
              last_modified(0), commit_info(nullptr), linked(nullptr) {}

        FragAction action;
        std::string fn;
        int64_t offset;
        int64_t len;
        int64_t last_modified;
        std::string md5sum;
        SCommitInfo* commit_info;
        std::unique_ptr<SFragInfo> linked;
        std::vector<SPunchItem> extra_exts;
    };

    typedef std::string (*common_prefix_func_t)(const std::string_view);
    typedef size_t (*common_prefix_hash_func_t)(const std::string_view);
    using on_delete_callback_t = std::function<std::vector<SingleFileStorage::SFragInfo>(const std::string&, const std::string&)>;
    using modify_data_callback_t = std::function<std::optional<std::string>(const std::string&, std::string, std::string)>;
    // Returns the new md5sum or an error code
    using modify_metadata_callback_t = std::function<nonstd::expected<std::string, int>(const std::string&, const std::string&, const std::string&)>;
    using match_callback_t = std::function<void(MatchInfo&, const SingleFileStorage::SFragInfo&)>;
    using add_reading_callback_t = std::function<void(const std::string_view fn, const SingleFileStorage::SFragInfo&)>;
    // Key of the counter counting objects with key fn. Empty if fn is not counted
    using counter_key_callback_t = std::function<std::string(const std::string& fn)>;
    // Whether the object fn with metadata md5sum is counted
    using counter_counts_callback_t = std::function<bool(const std::string& fn, const std::string& md5sum)>;

    struct SFSOptions
    {
        std::string data_path;
        std::string db_path;
        std::string freespace_cache_path;
        std::string dm_cache_path;
        std::string wal_file_path;
        int64_t dm_cache_size = 0;
        bool use_direct_io = false;
        int64_t data_file_size_limit_mb = 0;
        int64_t alloc_chunk_size = 512 * 1024 * 1024;
        // If this amount of free space is available in the data file,
        // use free space in data file instead of extending data file
        // Use >1 to disable
        float alloc_use_free_space_percent = 0.1;
        std::string runtime_id;
        bool manual_commit = false;
        bool stop_on_error = false;
        bool punch_holes = true;
        int64_t data_file_chunk_size = 10LL * 1024 * 1024 * 1024 * 1024;
        MDB_cmp_func* key_compare_func = nullptr;
        common_prefix_func_t common_prefix_func = nullptr;
        common_prefix_hash_func_t common_prefix_hash_func = nullptr;
        on_delete_callback_t on_delete_callback;
        modify_data_callback_t modify_data_callback;
        modify_metadata_callback_t modify_metadata_callback;
        match_callback_t match_callback;
        add_reading_callback_t add_reading_callback;
        // Counters: When adding/deleting an object fn the counter at counter_key_callback(fn) is updated in the same
        // transaction (by the change of counter_counts_callback). The counter value is a varint in the md5sum field
        // of the counter. Counters with value 0 are deleted
        counter_key_callback_t counter_key_callback;
        counter_counts_callback_t counter_counts_callback;
        bool wal_write_meta = true;
        int64_t wal_write_data = 0;
        // Do not fsync LMDB, data file and WAL file. For testing only, data may get lost/corrupted on power loss!
        bool no_sync = false;
    };

    SingleFileStorage(SFSOptions options);

    // Start with dead SFS
    SingleFileStorage();

    void operator=(const SingleFileStorage&) = delete;
    SingleFileStorage(SingleFileStorage&) = delete;

    ~SingleFileStorage();

    static void init_mutex();

    static void handle_mmap_read_error(void* addr);

    struct WritePrepareResult
    {
        int err;
        std::vector<Ext> extents;
    };

    WritePrepareResult write_prepare(const std::string& fn, size_t data_size);

    int write_ext(const Ext& ext, const char* data, size_t data_size, const bool complete_obj);

    int write_finalize(const std::string& fn, const std::vector<Ext>& extents, int64_t last_modified, const std::string_view md5sum,
                       bool no_del_old, bool is_fragment, std::unique_ptr<SFragInfo> linked = nullptr, MatchInfo* match_info = nullptr);

    int write(const std::string& fn,
              const char* data, size_t data_size, const size_t data_alloc_size,
              int64_t last_modified, const std::string_view md5sum,
              bool no_del_old, bool is_fragment, MatchInfo* match_info = nullptr);

    const static unsigned int ReadWithReadahead = 1;
    const static unsigned int ReadUnsynced = 2;
    const static unsigned int ReadMetaOnly = 4;
    const static unsigned int ReadSkipAddReading = 8;
    const static unsigned int ReadNewest = 16;
    const static unsigned int ReadAddReadingCallback = 32;
    // Read data via read-only mmap of the data file (falls back to pread on error)
    const static unsigned int ReadWithMmap = 64;
    // read_prepare: Fail with EWOULDBLOCK instead of reading via the commit thread if the item has modifications not
    // committed to LMDB yet (e.g. to not wait for the commit thread in an EventBase thread)
    const static unsigned int ReadFailUnsynced = 256;
    // Read the oldest version of the key. fn has to be the key with the smallest possible version (sorting last)
    const static unsigned int ReadOldest = 128;

    struct ReadPrepareResult
    {
        int err;
        std::vector<Ext> extents;
        int64_t total_len;
        std::string md5sum;
        std::string key;
        int64_t last_modified;
    };

    ReadPrepareResult read_prepare(const std::string_view fn, unsigned int flags);
    std::string read(const std::string& fn, unsigned int flags);
    ReadPrepareResult check_existence(const std::string_view fn, unsigned int flags);

    struct ReadExtResult
    {
        int err;
        std::unique_ptr<folly::IOBuf> buf;
    };

    ReadExtResult read_ext(const Ext& ext, const unsigned int flags, const size_t bufsize, folly::IOBufQueue& buf);

    int read_ext(const Ext& ext, const unsigned int flags, const size_t bufsize, char* bufptr, ssize_t& read);

    int read_finalize(const std::string& fn, const std::vector<Ext>& extents, unsigned int flags);

    int del(const std::string_view fn, DelAction da,
            bool background_queue, std::unique_ptr<SFragInfo> linked = nullptr, MatchInfo* match_info = nullptr);

    // Replaces the md5sum (metadata) of an existing object with the result of modify_metadata_callback
    // called with the current md5sum and param. Waits until the modification was applied.
    // Returns ENOENT if the object does not exist, otherwise the error code returned by the callback.
    int modify_metadata(const std::string& fn, const std::string_view param);

    bool restore_old(const std::string& fn);

    bool commit(bool background_queue, int64_t transid, const bool pre_sync)
    {
        return commit(background_queue, transid, 0, pre_sync);
    }

    bool commit(bool background_queue, int64_t transid, int64_t disk_id, const bool pre_sync);

    bool empty_queue(bool background_queue);

    struct IterData
    {
        MDB_txn* iter_txn;
        MDB_cursor* iter_cur;
        MDB_val iter_key;
        MDB_val iter_val;
    };

    bool iter_start(int64_t disk_id, bool compressed, IterData& iter_data);

    bool iter_start(bool compressed, IterData& iter_data);

    bool iter_start(std::string fn, bool compressed, IterData& iter_data);

    void start_debug();

    void iter_stop(IterData& iter_data);

    bool iter_next(IterData& iter_data);

    bool iter_curr_val(std::string& fn, int64_t& offset, int64_t& size, std::vector<SPunchItem>& exta_exts, int64_t& last_modified, std::string& md5sum, IterData& iter_data);

    bool iter_curr_val(std::string& fn, std::string& data, IterData& iter_data);

    /**
     * Iteration for listings with read after write consistency (when running with WAL or manual commit).
     * Like iter_start(fn, ...), but merges items modified since the last LMDB commit
     * into the iteration. Only modified items in the range are merged. The range starts at range_start
     * (in key order) and in_range has to return whether a key is in the range, so the range has to be
     * contiguous in key order. Modified items are copied in batches.
     * Use iter_curr_val with ListIterData::curr to get the current item.
     */
    struct ListIterData
    {
        // Current item (iter_key is nullptr at the end)
        IterData curr = {};

        // LMDB iteration
        IterData lmdb = {};
        bool lmdb_started = false;
        bool lmdb_at_end = false;
        // Generation of committed items erased from list_commit_items when the LMDB iteration was started
        uint64_t lmdb_generation = 0;
        // Generation when the batch of modified items was copied
        uint64_t modified_generation = 0;
        // Key the iteration started at
        std::string start_fn;

        std::function<bool(const std::string&)> in_range;

        // Batch of modified items in key order. Value is nullptr if deleted
        std::vector<std::pair<std::string, std::shared_ptr<const std::string>>> modified;
        // Position in modified. std::string::npos if there are no more modified items in range
        size_t modified_pos = 0;

        enum class Source
        {
            None,
            Lmdb,
            Modified,
            // Modified item replacing the LMDB item with the same key
            Both
        };

        // Key and source of the current item
        std::string curr_key;
        Source curr_source = Source::None;
    };

    bool list_iter_start(const std::string& fn, const std::string& range_start, std::function<bool(const std::string&)> in_range, ListIterData& iter_data);

    bool list_iter_next(ListIterData& iter_data);

    void list_iter_stop(ListIterData& iter_data);

    virtual void operator()();

    int64_t get_free_space_in_data_file();

    int64_t get_free_space_real();

    /**
     * Get amount of free space in the data file as percentage of data
     * file size. Underreports a bit (compared to get_free_space_in_data_file() )
     */
    float get_free_space_percent();

    int64_t get_total_space();

    int64_t get_data_file_size();

    int64_t get_data_file_size_full();

    int64_t max_free_extent(int64_t& len);

    int64_t get_free_space_slow(bool verbose, int64_t& freespace_extents, std::vector<SPunchItem>* items);

    bool check_len_idx();

    using str_map = std::map<std::string, std::string>;

    void defrag(str_map& params, relaxed_atomic<int64_t>& defrag_items);

    std::string get_db_path() { return db_path; }

    std::string get_cache_path() { return freespace_cache_path; }

    bool is_write_offline() { return write_offline; }

    bool start_thread(int64_t transid);

    int64_t get_transid()
    {
        std::scoped_lock lock(mutex);
        return curr_transid;
    }

    int64_t get_transid(int64_t disk_id);

    std::string meminfo();

    bool set_write_offline(bool b);

    bool get_is_dead() { return is_dead; }

    bool set_allow_defrag(bool b)
    {
        return set_allow_defrag(b, 0);
    }

    bool set_allow_defrag(bool b, int64_t disk_id);

    bool set_stop_defrag(bool b)
    {
        stop_defrag = b;
        return true;
    }

    bool reset_del_log(int64_t disk_id, int64_t reset_transid);

    bool reset_del_queue(int64_t disk_id, int64_t reset_transid);

    int64_t get_disk_id(const std::string& uuid);

    void migrate_thread();

    bool start_migrate();

    void reference();

    void unreference();

    static std::string decompress_filename(const std::string& fn);

    static int64_t get_fn_disk_id(const std::string& fn);

    static std::string remove_disk_id(const std::string& fn, size_t disk_id_size);

    std::string freespace_stats();

    virtual void wait_for_startup_finish();

    std::string get_runtime_id()
    {
        return runtime_id;
    }

    bool get_manual_commit()
    {
        return manual_commit;
    }

    int64_t get_next_partid();

    int64_t get_next_version();

    int64_t decrypt_id(const std::string& encdata);

    // Decrypts an object version id. Versions can be negative (prepended list items) or 0 (null version, written
    // before versioning was enabled). Returns std::nullopt if decryption fails
    std::optional<int64_t> decrypt_version(const std::string& encdata);

    std::string encrypt_id(const int64_t id);

    // Binary ETag (16 bytes) from a new version
    std::string get_next_version_etag();

    void free_extents(const std::vector<Ext>& extents);

    // Make an extent smaller by the specified amount from the end
    void remove_extent_end(int64_t offset, int64_t len);

    void wait_for_wal_startup_finished();

    void assert_reading_items_empty();

private:
    int write_int(const std::string& fn, const char* data, size_t data_size, const size_t data_alloc_size,
                  int64_t last_modified, const std::string_view md5sum, bool allow_defrag_lock, bool no_del_old, MatchInfo* match_info);

    int64_t remove_fn(const std::string& fn,
                      MDB_txn* txn, MDB_txn* freespace_txn, bool del_from_main, bool del_old, THREAD_ID tid, const bool call_callback, std::set<SPunchItem>* trim_items = nullptr);

    int64_t restore_fn(const std::string& fn,
                       MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid);

    int64_t log_fn(const std::string& fn,
                   MDB_txn* txn, THREAD_ID tid, int64_t transid);

    int64_t add_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid, int64_t offset, int64_t len, bool from_startup);

    int64_t rm_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid);

    void wait_queue(std::unique_lock<std::mutex>& lock, bool background_queue, bool defrag_check);

    bool add_freemap_ext(MDB_txn* txn, int64_t offset, int64_t len, bool used_in_curr_trans, THREAD_ID tid, std::set<SPunchItem>* trim_items = nullptr);

    bool add_freemap_ext_simple(MDB_txn* txn, int64_t offset, int64_t len, THREAD_ID tid);

    bool find_freemap_ext(MDB_txn* txn, THREAD_ID tid, int64_t& start, int64_t& len);

    void lock_defrag(const std::string& fn);

    bool is_defrag_skip_item(const std::string& fn);

    void unlock_defrag(const std::string& fn);

    void wait_defrag(const std::string_view fn, std::unique_lock<std::mutex>& lock);

    void setup_mmap_read_error(THREAD_ID tid);

    bool clear_mmap_read_error(THREAD_ID tid);

    bool has_mmap_read_error_reset(THREAD_ID tid);

    int64_t reset_del_log_fn(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid, int64_t disk_id, int64_t transid);

    int64_t reset_holes(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid);

    int64_t reset_del_queue(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid, int64_t disk_id, int64_t transid);

    void wait_startup_finished(std::unique_lock<std::mutex>& lock);

    int64_t get_really_min_space(int64_t& index_file_size);

    int64_t get_burn_in_data_size();

    bool do_free_minspace(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid);

    static std::mutex mmap_read_error_mutex;

    static std::unordered_map<THREAD_ID, std::pair<bool, std::vector<uintptr_t>>> mmap_read_error_jmp;
    static std::vector<MDB_env*> mmap_dbs;

    std::vector<uintptr_t> mmap_cleanup_addrs;

    int64_t queue_del(const std::string& fn, MDB_txn* txn, THREAD_ID tid, int64_t transid);

    int64_t unqueue_del(const std::string& fn, MDB_txn* txn, THREAD_ID tid);

    void add_defrag_skip_items_queue();

    bool open_cache_db(int64_t current_txn_id, int64_t mapsize, bool use_other, bool del_create, MDB_txn*& freespace_txn);

    bool generate_freespace_cache(MDB_txn* source_txn, MDB_txn* dst_txncs, bool fast_gen);

    bool freespace_check(MDB_txn* source_txn, MDB_txn* freespace_txn, bool fast_check);

    bool clear_freespace_cache(MDB_txn* txn);

    bool regen_datafile_free(MDB_txn* freespace_txn);

    bool regen_free_len_idx(MDB_txn* freespace_txn);

    class TmpMmapedPgIds
    {
    public:
        TmpMmapedPgIds();

        ~TmpMmapedPgIds();

        void add_pgid(size_t pgid)
        {
            if (n_pgids * sizeof(pgid) + sizeof(pgid) >= mmap_size)
            {
                std::cerr << "pgid mmap too small" << std::endl;
                abort();
            }
            memcpy(mmap_ptr + n_pgids * sizeof(pgid), &pgid, sizeof(pgid));
            ++n_pgids;
        }

        size_t size()
        {
            return n_pgids;
        }

        size_t* begin()
        {
            return reinterpret_cast<size_t*>(mmap_ptr);
        }

        size_t* end()
        {
            return reinterpret_cast<size_t*>(mmap_ptr + n_pgids * sizeof(size_t));
        }

        size_t& get(size_t idx)
        {
            return *(begin() + idx);
        }

    private:
        File backing_file;
        size_t n_pgids;
        size_t mmap_size;
        char* mmap_ptr;
    };

    bool read_pgids(MDB_txn* txn, MDB_dbi dbi, THREAD_ID tid, TmpMmapedPgIds& mmap_pg_ids);

    std::string compress_filename(const std::string& fn);

    SFragInfo get_frag_info(MDB_txn* txn, const std::string_view fn, bool parse_data, const bool read_newest, const bool read_oldest = false);

    bool generate_free_len_idx(MDB_txn* txn);

    int64_t get_disk_id(MDB_txn* txn, THREAD_ID tid, const std::string& uuid);

    int64_t get_disk_trans_id(MDB_txn* txn, THREAD_ID tid, int64_t disk_id);

    bool set_disk_trans_id(MDB_txn* txn, THREAD_ID tid, int64_t disk_id, int64_t trans_id);

    bool rewrite_npages(MDB_txn* txn, MDB_cursor* mc, THREAD_ID tid, size_t npages);

    int put_with_rewrite(MDB_txn* txn, MDB_dbi dbi, MDB_val* tkey, MDB_val* tval, THREAD_ID tid, size_t npages);

    int put_frag_info_md5sum(MDB_txn* txn, THREAD_ID tid, const std::string& fn, const SFragInfo& frag_info, const std::string& md5sum);

    // Key of the counter of fn (see SFSOptions::counter_key_callback). Empty if not counted
    std::string counter_key(const std::string& fn);

    // Updates the counter counter_fn after object fn changed from old_info (offset -1 if it did not exist) to
    // new_md5sum (nullptr if deleted). Returns number of errors
    int64_t update_counter(MDB_txn* txn, THREAD_ID tid, const std::string& counter_fn, const std::string& fn,
                           const SFragInfo& old_info, const std::string* new_md5sum);

    // Register/unregister a read of an object
    void add_reading_item(const SFragInfo& fi);

    void remove_reading_item(const std::vector<Ext>& extents);

    void do_stop_on_error();

    bool setup_compare_funcs(MDB_txn* txn, MDB_txn* freespace_txn);

    int add_new_object(MDB_txn* txn, const THREAD_ID tid, const std::string& fn);

    bool frag_info_matches(MDB_txn* txn, SFragInfo& frag_info);

    bool run_trim_queue(std::set<SPunchItem>& trim_items);

    void run_trim_thread();

    bool has_commit_item(const FragAction action);

    // Whether modified items are tracked in list_commit_items (for merging them into listings, see
    // list_iter_start) instead of commit_items. Necessary if items are not committed to the LMDB directly
    // (WAL or manual commit)
    bool list_commit_items_used() const;

    // Count a queued modification of fn in commit_items/list_commit_items. Requires mutex to be locked
    void incr_commit_item(const std::string_view fn);
    // Without the counter of fn
    void incr_commit_item_key(const std::string_view fn);
    void decr_commit_item_key(const std::string_view fn);

    // Mark a queued modification of fn as done (it stays modified until the next LMDB commit).
    // Requires mutex to be locked
    void decr_commit_item(const std::string_view fn);

    // Whether fn was modified since the last LMDB commit. Requires mutex to be locked
    bool is_commit_item(const std::string_view fn);

    // Whether there are modified items since the last LMDB commit. Requires mutex to be locked
    bool commit_items_empty() const;

    // Remove items without queued modifications after an LMDB commit. Requires mutex to be locked
    void erase_committed_items();

    void queue_linked(const SFragInfo& frag_info);

    bool with_rewrite;

    std::unordered_set<std::string> defrag_skip_items;
    bool is_defragging;
    int defrag_restart;
    std::atomic<bool> stop_defrag;
    bool allow_defrag;
    std::set<int64_t> disallow_defrag_disk_id;
    std::deque<SFragInfo> commit_queue;
    std::deque<SFragInfo> commit_background_queue;
    std::deque<SFragInfo> callback_queue;
    std::thread commit_thread_h;
    // Hashed common prefixes (see common_prefix_hash_func) of items modified since the last LMDB commit, with
    // the number of queued modifications. Entries are removed once committed to the LMDB. Only used if
    // list_commit_items is not used (see list_commit_items_used)
    std::unordered_map<size_t, size_t> commit_items;

    // Compares keys with the key compare function (if set), so list_commit_items is in key order
    struct CommitItemsCompare
    {
        MDB_cmp_func* key_compare_func = nullptr;

        bool operator()(const std::string& a, const std::string& b) const
        {
            if (key_compare_func == nullptr)
                return a < b;

            MDB_val va = { a.size(), const_cast<char*>(a.data()) };
            MDB_val vb = { b.size(), const_cast<char*>(b.data()) };
            return key_compare_func(&va, &vb) < 0;
        }
    };

    struct ListCommitItem
    {
        // Number of queued modifications
        size_t refs = 0;
        // Value in the LMDB after the last applied modification. Unset if no modification was applied yet,
        // nullptr if the item was deleted
        std::optional<std::shared_ptr<const std::string>> value;
    };

    // Used instead of commit_items if listings need a commit (list_commit_items_used()). Keys modified since
    // the last LMDB commit in key order, for merging them into listings (see list_iter_start)
    std::map<std::string, ListCommitItem, CommitItemsCompare> list_commit_items;
    // Incremented each time committed items are erased from list_commit_items
    uint64_t list_commit_items_generation = 0;

    // Number of entries in commit_items/list_commit_items (updated with mutex locked). Lets reads skip locking the
    // mutex to check whether they have to read unsynced (is_commit_item) if there are no modified items
    std::atomic<size_t> commit_items_count{ 0 };

    // Incremented by the commit thread before freed extents can be reused/trimmed (and before committed items are
    // erased). A read whose lookup and registration did not overlap with an increment registered in time
    std::atomic<uint64_t> free_generation{ 0 };

    // read_prepare (internal): Read was already retried because extents were freed during the lookup
    const static unsigned int ReadRetried = 1u << 30;

    // Updates commit_items_count. Requires mutex to be locked
    void update_commit_items_count();

    // Set the value of fn in list_commit_items after applying a modification (in commit thread)
    void set_list_commit_item_value(MDB_txn* txn, const std::string& fn);

    // Copy the next batch of modified items for list_iter. Starts at start (inclusive) if set, otherwise
    // after iter_data.modified's last item. Sets modified_pos to std::string::npos if there are no more
    // modified items in range
    bool list_iter_get_modified(const std::string* start, ListIterData& iter_data);

    // Start or restart LMDB iteration for list_iter at fn (with skip_fn, skip that item)
    bool list_iter_start_lmdb(const std::string& fn, bool skip_fn, ListIterData& iter_data);

    // Set the current item of list_iter to the next item in key order
    bool list_iter_set_curr(ListIterData& iter_data);

    int key_compare(const std::string_view a, const std::string_view b) const;

    bool do_quit;

    bool startup_finished;

    int64_t data_file_max_size;
    int64_t data_file_offset;
    int64_t data_file_offset_end;
    // End of the data file space preallocated by write_prepare (datafileoffset_mutex). data_file_max_size is
    // owned by the commit thread
    int64_t data_file_alloc_end = 0;
    int64_t data_file_free;
    std::map<int64_t, int64_t> reserved_extents;
    std::mutex reserved_extents_mutex;
    MultiFile data_file;
    MultiFile data_file_dio;
    MultiFile new_data_file;
    MultiFile new_data_file_dio;
    MDB_env* db_env;
    MDB_dbi dbi_main;
    MDB_dbi dbi_free;
    MDB_dbi dbi_free_len;
    MDB_dbi dbi_size;
    MDB_dbi dbi_old;
    MDB_dbi dbi_holes;
    MDB_dbi dbi_queue_del;
    MDB_dbi dbi_new_objects;
    MDB_env* cache_db_env;
    MDB_dbi dbi_cache_size;
    std::set<int64_t> curr_new_free_extents;
    // Extents freed while being read. Not reused/trimmed until the read finished. Rarely filled. Lock order: reading
    // shard mutex before reading_free_skip_mutex
    std::mutex reading_free_skip_mutex;
    std::set<int64_t> reading_free_skip_extents;
    std::unordered_set<std::string> defrag_items;
    std::vector<SPunchItem> curr_free_skip_extents;

    struct ReadingItem
    {
        size_t refs = 0;
        bool free_skip = false;
    };

    // Registered reads (by first data file offset of the object), sharded by offset so reads of different objects
    // don't contend
    static constexpr size_t num_reading_shards = 64;
    struct alignas(64) ReadingShard
    {
        std::mutex mutex;
        std::map<int64_t, ReadingItem> items;
    };
    std::array<ReadingShard, num_reading_shards> reading_shards;

    ReadingShard& reading_shard(const int64_t offset)
    {
        return reading_shards[static_cast<uint64_t>(offset / sfs_block_size) % num_reading_shards];
    }

    size_t num_reading_items();

    std::mutex mutex;
    std::condition_variable cond;

    std::mutex datafileoffset_mutex;
    std::mutex freespace_mutex;

    int64_t min_free_space;

    std::string db_path;

    std::string freespace_cache_path;

    relaxed_atomic<bool> is_dead;
    relaxed_atomic<bool> write_offline;
    std::atomic<bool> data_file_dirty;

    int64_t prev_transid;
    int64_t curr_transid;

    bool force_freespace_check;

    bool regen_freespace_cache;
    bool sync_freespace_cache;

    int64_t next_disk_id;

    int64_t data_file_copy_done;
    int64_t data_file_copy_done_sync;
    int64_t data_file_copy_max;
    bool stop_data_file_copy;
    std::shared_mutex data_file_copy_mutex;
    std::thread migrate_thread_h;
    relaxed_atomic<int> references;

    bool mdb_curr_sync;

    std::filesystem::path data_file_path;

    int64_t data_file_size_limit;
    int64_t alloc_chunk_size;
    float alloc_use_free_space_percent;
    int64_t data_file_chunk_size;

    std::string runtime_id;
    bool manual_commit;
    bool stop_on_error;
    bool punch_holes;
    bool no_sync;

    int64_t curr_partid = 0;
    unsigned char enckey[crypto_secretbox_KEYBYTES];

    MDB_cmp_func* key_compare_func;

    common_prefix_func_t common_prefix_func;
    common_prefix_hash_func_t common_prefix_hash_func;
    on_delete_callback_t on_delete_callback;
    modify_data_callback_t modify_data_callback;
    modify_metadata_callback_t modify_metadata_callback;
    match_callback_t match_callback;
    add_reading_callback_t add_reading_callback;
    counter_key_callback_t counter_key_callback;
    counter_counts_callback_t counter_counts_callback;

    int64_t curr_version = 0;

    std::unique_ptr<WalFile> wal_file;
    bool wal_write_meta = true;
    int64_t wal_write_data = 0;
    char wal_uuid[16];
    size_t startup_wal_items = 0;
    bool wal_startup_finished = true;
    std::condition_variable wal_startup_finished_cond;
    std::jthread wal_write_thread;

    std::vector<std::unique_ptr<SingleFileStorage::SCommitInfo>> wal_startup_commit_infos;
    std::vector<std::unique_ptr<SingleFileStorage::MatchInfo>> wal_startup_match_infos;

    std::condition_variable trim_queue_cond;
    std::queue<SPunchItem> trim_queue;
    std::mutex trim_queue_mutex;
    std::map<int64_t, int> trimming_free_skip_extents;

    std::jthread trim_thread;
};

class ScopedSFSRef
{
    SingleFileStorage* sfs;

public:
    ScopedSFSRef(SingleFileStorage* sfs)
        : sfs(sfs)
    {
        if (sfs != nullptr)
            sfs->reference();
    }

    void reset(SingleFileStorage* nsfs)
    {
        if (sfs != nullptr)
            sfs->unreference();
        sfs = nsfs;
        if (sfs != nullptr)
            sfs->reference();
    }

    ~ScopedSFSRef()
    {
        if (sfs != nullptr)
            sfs->unreference();
    }
};
