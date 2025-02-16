#pragma once
#include <folly/io/IOBuf.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <queue>
#include <memory.h>
#include "lmdb/lmdb.h"
#include <assert.h>
#include "relaxed_atomic.h"
#include <sys/types.h>
#include <mutex>
#include <condition_variable>
#include <shared_mutex>
#include <iostream>
#include <stdint.h>
#include <filesystem>
#include <thread>
#include <folly/io/IOBufQueue.h>
#include <sodium.h>
#include "File.h"


using THREAD_ID = pid_t;

constexpr int64_t sfs_block_size = 4096;

class SingleFileStorage
{
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
		AssertQueueEmpty = 5
	};

	typedef std::string(*common_prefix_func_t)(const std::string_view);
	typedef size_t(*common_prefix_hash_func_t)(const std::string_view);

	struct SFSOptions
	{
		std::string data_path;
		std::string db_path;
		std::string freespace_cache_path;
		std::string dm_cache_path;
		int64_t dm_cache_size = 0;
		bool use_direct_io = false;
		int64_t data_file_size_limit_mb = 0;
		int64_t alloc_chunk_size = 512 * 1024 * 1024;
		std::string runtime_id;
		bool manual_commit = false;
		bool stop_on_error = false;
		bool punch_holes = true;
		int64_t data_file_chunk_size = 10LL * 1024 * 1024 * 1024 * 1024;
		MDB_cmp_func* key_compare_func = nullptr;
		common_prefix_func_t common_prefix_func = nullptr;
		common_prefix_hash_func_t common_prefix_hash_func = nullptr;
	};

	SingleFileStorage(SFSOptions options);

	//Start with dead SFS
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

	int write_ext(const Ext& ext, const char* data, size_t data_size);

	int write_finalize(const std::string& fn, const std::vector<Ext>& extents, int64_t last_modified, const std::string& md5sum,
		const std::string& metadata, bool no_del_old, bool is_fragment);

	int write(const std::string& fn,
		const char* data, size_t data_size, const size_t data_alloc_size, 
		int64_t last_modified, const std::string& md5sum, const std::string& metadata,
		bool no_del_old, bool is_fragment);

	const static unsigned int ReadWithReadahead = 1;
	const static unsigned int ReadUnsynced = 2;
	const static unsigned int ReadMetaOnly = 4;
	const static unsigned int ReadSkipAddReading = 8;
	const static unsigned int ReadNewest = 16;

	struct ReadPrepareResult
	{
		int err;
		std::vector<Ext> extents;
		int64_t total_len;
		std::string md5sum;
		std::string key;
	};

	ReadPrepareResult read_prepare(const std::string_view fn, unsigned int flags);
	std::string read(const std::string& fn, unsigned int flags);
	int check_existence(const std::string_view fn, unsigned int flags);

	struct ReadExtResult
	{
		int err;
		std::unique_ptr<folly::IOBuf> buf;
	};

	ReadExtResult read_ext(const Ext& ext, const unsigned int flags, const size_t bufsize, folly::IOBufQueue& buf);

	int read_finalize(const std::string& fn, const std::vector<Ext>& extents, unsigned int flags);

	int del(const std::string_view fn, DelAction da,
		bool background_queue);

	bool restore_old(const std::string& fn);


	bool commit(bool background_queue, int64_t transid) {
		return commit(background_queue, transid, 0);
	}

	bool commit(bool background_queue, int64_t transid, int64_t disk_id);

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

	bool iter_curr_val(std::string& fn, int64_t& offset, int64_t& size, std::vector<SPunchItem>& exta_exts,
		int64_t& last_modified, std::string& md5sum, const std::string& metadata, IterData& iter_data);

	bool iter_curr_val(std::string& fn, std::string& data, IterData& iter_data);

	virtual void operator()();

	int64_t get_free_space_in_data_file();

	int64_t get_free_space_real();

	int64_t get_total_space();

	int64_t get_data_file_size();

	int64_t max_free_extent(int64_t& len);

	int64_t get_free_space_slow(bool verbose, int64_t& freespace_extents, std::vector<SPunchItem>* items);

	bool check_len_idx();

	using str_map = std::map<std::string, std::string>;

	void defrag(str_map& params, relaxed_atomic<int64_t>& defrag_items);

	std::string get_db_path() { return db_path; }

	std::string get_cache_path() { return freespace_cache_path; }

	bool is_write_offline() { return write_offline; }

	bool start_thread(int64_t transid);

	int64_t get_transid() {
        std::scoped_lock lock(mutex);
		return curr_transid;
	}

	int64_t get_transid(int64_t disk_id);

	std::string meminfo();

	bool set_write_offline(bool b);

	bool get_is_dead() { return is_dead; }

	bool set_allow_defrag(bool b) {
		return set_allow_defrag(b, 0);
	}

	bool set_allow_defrag(bool b, int64_t disk_id);

	bool set_stop_defrag(bool b) { stop_defrag = b; return true; }

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

	std::string get_runtime_id() {
		return runtime_id;
	}

	bool get_manual_commit() {
		return manual_commit;
	}

    std::pair<int64_t, std::string> get_next_partid();

	int64_t get_next_version();

	int64_t decrypt_id(const std::string& encdata);

	std::string encrypt_id(const int64_t id);

private:

	int write_int(const std::string& fn, const char* data, size_t data_size, const size_t data_alloc_size,
		int64_t last_modified, const std::string& md5sum, const std::string& metadata, bool allow_defrag_lock, bool no_del_old);

	int64_t remove_fn(const std::string& fn,
		MDB_txn* txn, MDB_txn* freespace_txn, bool del_from_main, bool del_old, THREAD_ID tid);

	int64_t restore_fn(const std::string& fn,
		MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid);

	int64_t log_fn(const std::string& fn,
		MDB_txn* txn, THREAD_ID tid, int64_t transid);

	int64_t add_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid, int64_t offset, int64_t len);

	int64_t rm_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid);

	void wait_queue(std::unique_lock<std::mutex>& lock, bool background_queue, bool defrag_check);

	bool add_freemap_ext(MDB_txn* txn, int64_t offset, int64_t len, bool used_in_curr_trans, THREAD_ID tid);

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

	void free_extents(const std::vector<Ext>& extents);

	int64_t get_really_min_space(int64_t& index_file_size);

	int64_t get_burn_in_data_size();

	bool do_free_minspace(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid);

	static std::mutex mmap_read_error_mutex;

	static std::unordered_map<THREAD_ID, std::pair<bool, std::vector<uintptr_t> > > mmap_read_error_jmp;
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

	enum class FragAction
	{
		Add,
		Del,
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
		AssertDelQueueEmpty
	};

	struct SFragInfo;

	struct SCommitInfo
	{
		SCommitInfo()
			: commit_errors(0),
			frag_info(nullptr)
		{}
		int64_t commit_errors;
		std::condition_variable commit_done;
		int64_t new_datafile_offset;
		int64_t new_datafile_offset_end;
		SFragInfo* frag_info;
	};

	struct SFragInfo
	{
		SFragInfo() : offset(-1), len(0),
			last_modified(0), commit_info(nullptr) {
		}
		SFragInfo(int64_t offset, int64_t len)
			: offset(offset), len(len),
			last_modified(0), commit_info(nullptr) {}

		FragAction action;
		std::string fn;
		int64_t offset;
		int64_t len;
		int64_t last_modified;
		std::string md5sum;
		std::string metadata;
		SCommitInfo* commit_info;
		std::vector<SPunchItem> extra_exts;
	};

	std::string compress_filename(const std::string& fn);

	SFragInfo get_frag_info(MDB_txn* txn, const std::string_view fn, bool parse_data, const bool read_newest);

	bool generate_free_len_idx(MDB_txn* txn);

	int64_t get_disk_id(MDB_txn * txn, THREAD_ID tid, const std::string& uuid);

	int64_t get_disk_trans_id(MDB_txn * txn, THREAD_ID tid, int64_t disk_id);

	bool set_disk_trans_id(MDB_txn * txn, THREAD_ID tid, int64_t disk_id, int64_t trans_id);

	bool rewrite_npages(MDB_txn* txn, MDB_cursor* mc, THREAD_ID tid, size_t npages);

	int put_with_rewrite(MDB_txn* txn, MDB_dbi dbi, MDB_val* tkey, MDB_val* tval, THREAD_ID tid, size_t npages);

	void add_reading_item(const SFragInfo& fi);

	void remove_reading_item(const std::vector<Ext>& extents);

	void do_stop_on_error();

	bool setup_compare_funcs(MDB_txn* txn, MDB_txn* freespace_txn);

	bool with_rewrite;

	std::unordered_set<std::string> defrag_skip_items;
	bool is_defragging;
	int defrag_restart;
	std::atomic<bool> stop_defrag;
	bool allow_defrag;
	std::set<int64_t> disallow_defrag_disk_id;
	std::deque<SFragInfo> commit_queue;
	std::deque<SFragInfo> commit_background_queue;
	std::thread commit_thread_h;
	std::unordered_map<size_t, size_t> commit_items;

	bool do_quit;

	bool startup_finished;

	int64_t data_file_max_size;
	int64_t data_file_offset;
	int64_t data_file_offset_end;
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
	MDB_env* cache_db_env;
	MDB_dbi dbi_cache_size;
	std::set<int64_t> curr_new_free_extents;
	std::set<int64_t> reading_free_skip_extents;
	std::unordered_set<std::string> defrag_items;
	std::vector<SPunchItem> curr_free_skip_extents;

	struct ReadingItem
	{
		size_t refs = 0;
		bool free_skip = false;
	};

	std::map<int64_t, ReadingItem> reading_items;

    std::mutex mutex;
    std::condition_variable cond;

	std::mutex datafileoffset_mutex;
	std::mutex freespace_mutex;

	int64_t min_free_space;

	std::string db_path;

	std::string freespace_cache_path;

	relaxed_atomic<bool> is_dead;
	relaxed_atomic<bool> write_offline;

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
	int64_t data_file_chunk_size;

	std::string runtime_id;
	bool manual_commit;
	bool stop_on_error;
	bool punch_holes;

	int64_t curr_partid = 0;
	unsigned char enckey[crypto_secretbox_KEYBYTES];

	MDB_cmp_func* key_compare_func;

	common_prefix_func_t common_prefix_func;
	common_prefix_hash_func_t common_prefix_hash_func;

	int64_t curr_version = 0;
};


class ScopedSFSRef
{
	SingleFileStorage* sfs;
public:
	ScopedSFSRef(SingleFileStorage* sfs)
		: sfs(sfs) {
		if(sfs!=nullptr)
			sfs->reference();
	}

	void reset(SingleFileStorage* nsfs)
	{
		if (sfs != nullptr) sfs->unreference();
		sfs = nsfs;
		if (sfs != nullptr) sfs->reference();
	}

	~ScopedSFSRef() {
		if (sfs != nullptr)
			sfs->unreference();
	}
};
