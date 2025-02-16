/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "SingleFileStorage.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <cstdint>
#include <folly/File.h>
#include <folly/FileUtil.h>
#include <folly/Range.h>
#include <folly/io/IOBuf.h>
#include <memory.h>
#include <functional>
#include <mutex>
#include <set>
#include <limits.h>
#include <stdexcept>
#include <folly/Random.h>
#include <folly/String.h>
#include <folly/Exception.h>
#include <folly/logging/LogLevel.h>
#include <folly/logging/xlog.h>
#include <folly/ScopeGuard.h>
#include <folly/io/IOBufQueue.h>
#include "lmdb/lmdb.h"
#include "os_functions.h"
#include "utils.h"
#include <filesystem>
#include <cstdlib>

#include <string>
#include <system_error>
#include <thread>
#include <chrono>
#include "data.h"
#include "crypt.h"

using namespace std::chrono_literals;


#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mntent.h>
#else
#define aligned_alloc(allign, size) malloc(size)
int getpagesize() { return 4096; }
const int PROT_READ = 1;
const int PROT_WRITE = 2;
const int MAP_SHARED = 2;
const int MS_SYNC = 1;
void* mmap(void* addr, size_t length, int prot, int flags,
	void* fd, off_t offset) {
	return nullptr;
}
int munmap(void* addr, size_t length) {
	return 0;
}
int msync(void* addr, size_t length, int flags);
typedef int64	ssize_t;
int fallocate64(void* fd, int mode, int64_t offset, int64_t len) {
	return 1;
}
#endif
#include <signal.h>
#include "relaxed_atomic.h"

#define EXT_DEBUG(x)

namespace
{
	int64_t div_up(int64_t num, int64_t div)
	{
		if (num%div == 0)
		{
			return num / div;
		}
		else
		{
			return num / div + 1;
		}
	}

	const int64_t block_size = 4096;
	const size_t num_cached_free_exts = 1000;
	const size_t max_defrag_extents = 1000000;
	const int64_t data_file_copy_num_bytes = 100 * 1024 * 1024;
	const size_t n_rewrite_pages = (128 * 1024) / 4096;

	/** Compare two varint lexically */
	static int
		mdb_cmp_varint(const MDB_val *a, const MDB_val *b)
	{
		if (a->mv_size != b->mv_size)
		{
			return a->mv_size < b->mv_size ? -1 : 1;
		}

		return memcmp(a->mv_data, b->mv_data, a->mv_size);
	}

	static int
		mdb_cmp_varint_rev(const MDB_val *a, const MDB_val *b)
	{
		return -1 * mdb_cmp_varint(a, b);
	}

	static int
		mdb_cmp_memn(const MDB_val* a, const MDB_val* b)
	{
		int diff;
		ssize_t len_diff;
		unsigned int len;

		len = a->mv_size;
		len_diff = (ssize_t)a->mv_size - (ssize_t)b->mv_size;
		if (len_diff > 0) {
			len = b->mv_size;
			len_diff = 1;
		}

		diff = memcmp(a->mv_data, b->mv_data, len);
		return static_cast<int>(diff ? diff : len_diff < 0 ? -1 : len_diff);
	}

	const int64_t max_inline_exts = 511;

	int64_t extract_num_exts(int64_t& offset)
	{
		int64_t exts = offset % 512;
		offset -= exts;
		return exts;
	}

	int64_t encode_num_exts(int64_t offset, int64_t exts)
	{
		assert(exts < max_inline_exts);
		assert(offset % 512 == 0);
		return offset + exts;
	}

	int64_t encode_max_num_exts(int64_t offset)
	{
		return offset + max_inline_exts;
	}

	bool read_extra_exts(int64_t& offset, CRData& rdata,
		std::vector<SingleFileStorage::SPunchItem>& extra_exts)
	{
		int64_t num_exts = extract_num_exts(offset);

		if(num_exts==max_inline_exts)
		{
			int64_t skip_lists;
			if(!rdata.getVarInt(&num_exts) ||
				!rdata.getVarInt(&skip_lists))
			{
				XLOG(ERR) << "Error reading number of extents/skip lists";
				return false;
			}
		}

		extra_exts.resize(num_exts);

		for (int64_t i = 0; i < num_exts; ++i)
		{
			auto& ext = extra_exts[i];
			if (!rdata.getVarInt(&ext.offset)
				|| !rdata.getVarInt(&ext.len))
			{
				XLOG(ERR) << "Error reading extent " << i << " information";
				return false;
			}
		}

		return true;
	}

	class SingleFileStorageMigrate
	{
		SingleFileStorage* sfs;
	public:
		SingleFileStorageMigrate(SingleFileStorage* sfs)
			:sfs(sfs) {}

		void operator()()
		{
			sfs->migrate_thread();
			delete this;
		}
	};

	bool writeZeroes(int64_t offset, const File& file, int64_t num, int set_val)
	{
		static char buf[4096] = {};

		while (num > 0)
		{
			int64_t towrite = (std::min)((int64_t)4096, num);
			if (file.pwriteFull(buf, static_cast<size_t>(towrite), offset)!= towrite)
			{
				return false;
			}
			num -= towrite;
			offset += towrite;
		}
		return true;
	}

	

	class TmpMmapedFileBitmap
	{
	public:
		TmpMmapedFileBitmap(int64_t n, bool init_set)
			: mmap_ptr(nullptr), backing_file(folly::File::temporary())
		{
			resize(n, init_set);
		}

		~TmpMmapedFileBitmap()
		{
			if (mmap_ptr != nullptr)
			{
				munmap(mmap_ptr, bitmap_size);
			}
		}

		void resize(int64_t n, bool init_set)
		{
			if (mmap_ptr != nullptr)
			{
				munmap(mmap_ptr, bitmap_size);
				mmap_ptr = nullptr;
			}

			total_size = n;
			bitmap_size = static_cast<size_t>(n / 8 + (n % 8 == 0 ? 0 : 1));

			if (backing_file.size() < static_cast<int64_t>(bitmap_size))
			{
				while (!writeZeroes(backing_file.size(), backing_file, bitmap_size - backing_file.size(), init_set ? 255 : 0))
				{
                    XLOG(ERR) << "Error resizing bitmap file. Retrying...";
                    std::this_thread::sleep_for(1s);
				}
			}

			if(bitmap_size==0)
				return;

			mmap_ptr = reinterpret_cast<char*>(mmap(NULL, bitmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, backing_file.fd(), 0));
			
			if (mmap_ptr == reinterpret_cast<char*>(-1))
			{
                XLOGF(ERR, "Error creating mmap of bitmap. {}", folly::errnoStr(errno));
				abort();
			}
		}

		void set(int64_t i, bool v)
		{
			size_t bitmap_byte = (size_t)(i / 8);
			size_t bitmap_bit = i % 8;

			unsigned char b = mmap_ptr[bitmap_byte];

			if (v == true)
				b = b | (1 << (7 - bitmap_bit));
			else
				b = b & (~(1 << (7 - bitmap_bit)));

			mmap_ptr[bitmap_byte] = b;
		}

		size_t set_range(int64_t start, int64_t end, bool v)
		{
			size_t set_bits = 0;
			for (; start < end; ++start)
			{
				if (get(start) != v)
				{
					set(start, v);
					++set_bits;
				}
			}
			return set_bits;
		}

		char getb(int64_t i) const
		{
			size_t bitmap_byte = (size_t)(i / 8);
			return mmap_ptr[bitmap_byte];
		}

		bool get(int64_t i) const
		{
			size_t bitmap_byte = (size_t)(i / 8);
			size_t bitmap_bit = i % 8;

			unsigned char b = mmap_ptr[bitmap_byte];

			bool has_bit = ((b & (1 << (7 - bitmap_bit))) > 0);

			return has_bit;
		}

		bool flush()
		{
			if(mmap_ptr==nullptr)
				return true;

			return msync(mmap_ptr, bitmap_size, MS_SYNC) == 0;
		}

		size_t count_bits()
		{
			size_t set_count = 0;
			for (int64_t i = 0; i < total_size;)
			{
				if (i % 8 == 0
					&& getb(i) == 0)
				{
					i += 8;
					continue;
				}

				if (get(i))
				{
					++set_count;
				}

				++i;
			}

			return set_count;
		}

		bool get_range(int64_t start, int64_t end) const
		{
			for (; start < end; ++start)
			{
				if (get(start))
				{
					return true;
				}
			}
			return false;
		}

		int64_t size()
		{
			return total_size;
		}

		size_t meminfo()
		{
			return bitmap_size;
		}


	private:
		File backing_file;
		size_t bitmap_size;
		int64_t total_size;
		char* mmap_ptr;
	};

	const char dbi_size_info_size = 0;
	const char dbi_size_info_next_disk_id = 1;
	const char dbi_size_info_migration = 2;
	const char dbi_size_info_ext_freespace = 3;
	const char dbi_size_info_enckey = 4;

}

#ifndef _WIN32
void mmap_read_error(int sig, siginfo_t *si, void *unused)
{
	if (sig != SIGBUS)
	{
		abort();
	}
	SingleFileStorage::handle_mmap_read_error(si->si_addr);
}
#endif

namespace
{
	std::string common_prefix_passthrough(const std::string_view str)
	{
		return std::string(str);
	}

	size_t common_prefix_hash_func_passthrough(const std::string_view str)
	{
		return std::hash<std::string_view>()(str);
	}
}

std::mutex SingleFileStorage::mmap_read_error_mutex;
std::unordered_map<THREAD_ID, std::pair<bool, std::vector<uintptr_t> > > SingleFileStorage::mmap_read_error_jmp;
std::vector<MDB_env*> SingleFileStorage::mmap_dbs;

SingleFileStorage::SingleFileStorage(SFSOptions options)
    : data_file(options.data_path + os_file_sep() + "data", options.data_file_chunk_size, O_RDWR|O_CREAT|O_CLOEXEC),
    data_file_path(options.data_path + os_file_sep() + "data"),
	data_file_max_size(0), data_file_offset(0), data_file_offset_end(-1), data_file_free(0), do_quit(false), 
	min_free_space(20LL * 1024 * 1024 * 1024), is_defragging(false), defrag_restart(0), db_path(options.db_path),
	is_dead(false), write_offline(false), curr_transid(1), startup_finished(false),
	force_freespace_check(true), stop_defrag(false), allow_defrag(true), next_disk_id(1), data_file_copy_done(-1), data_file_copy_max(0), data_file_copy_done_sync(0),
	stop_data_file_copy(false), references(0), 	db_env(nullptr), freespace_cache_path(options.freespace_cache_path), cache_db_env(nullptr), regen_freespace_cache(false),
	sync_freespace_cache(true), mdb_curr_sync(false), data_file_size_limit(options.data_file_size_limit_mb*1024*1024), alloc_chunk_size(options.alloc_chunk_size),
	runtime_id(options.runtime_id), manual_commit(options.manual_commit), stop_on_error(options.stop_on_error), punch_holes(options.punch_holes),
	data_file_chunk_size(options.data_file_chunk_size), key_compare_func(options.key_compare_func), common_prefix_func(options.common_prefix_func),
	common_prefix_hash_func(options.common_prefix_hash_func)
{
	if(common_prefix_func==nullptr)
		common_prefix_func = common_prefix_passthrough;
	if(common_prefix_hash_func==nullptr)
		common_prefix_hash_func = common_prefix_hash_func_passthrough;

	int64_t index_file_size = 0;

	int64_t total_space = os_total_space(options.data_path);
	if (total_space > 0 && total_space < 100LL * 1024 * 1024 * 1024)
	{
		min_free_space = 2LL * 1024 * 1024 * 1024;
        XLOGF(INFO, "Minimum free space: {}", folly::prettyPrint(min_free_space, folly::PRETTY_BYTES_IEC));
	}

	int64_t mapsize = total_space / 175;

	if (mapsize < 1LL * 1024 * 1024 * 1024)
	{
		mapsize = 1LL * 1024 * 1024 * 1024;
	}

	mapsize = ((mapsize + 1 * 1024 * 1024) / (1 * 1024 * 1024)) * 1 * 1024 * 1024;

	std::string index_lmdb_fn = db_path + os_file_sep() + "index.lmdb";

    XLOGF(INFO, "{} max size: {}", index_lmdb_fn, folly::prettyPrint(mapsize * 2, folly::PRETTY_BYTES_IEC));

	{
        int fd = open(index_lmdb_fn.c_str(), O_RDONLY | O_CLOEXEC);
		if (fd != -1)
		{
			index_file_size = fileSize(fd);
		}
		else
		{
			if (std::filesystem::exists(options.data_path + os_file_sep() + "active"))
			{
				throw std::runtime_error("File storage was active before (file "+ options.data_path + os_file_sep() 
					+ "active present) but there is no index file at "+ index_lmdb_fn +" (index_file_not_found)");
			}

			MDB_env* tmp_env;
			int rc = mdb_env_create(&tmp_env);
			if (rc)
			{
				throw std::runtime_error("LMDB(0): Failed to create LMDB env (" + (std::string)mdb_strerror(rc) + ")");
			}

			rc = mdb_env_set_mapsize(tmp_env, mapsize * 2);

			if (rc)
			{
				throw std::runtime_error("LMDB(0): Failed to set map size (" + (std::string)mdb_strerror(rc) + ")");
			}

			rc = mdb_env_open(tmp_env, index_lmdb_fn.c_str(), MDB_NOSUBDIR|MDB_NOLOCK, 0664);

			if (rc)
			{
				throw std::runtime_error("LMDB(0): Failed to open LMDB database file (" + (std::string)mdb_strerror(rc) + ")");
			}

			mdb_env_close(tmp_env);

            fd = open(index_lmdb_fn.c_str(), O_RDONLY | O_CLOEXEC);
			if (fd != -1)
			{
				index_file_size = fileSize(fd);
			}
			else
			{
				throw std::runtime_error("LMDB(0): Cannot open index file to get size");
			}
		}

		if (!(os_get_file_type(db_path + os_file_sep() + "index.lmdb-lock") & EFileType_Symlink))
		{
            std::filesystem::remove(db_path + os_file_sep() + "index.lmdb-lock");
			if (symlink(("/tmp/index.lmdb-lock-" + random_uuid()).c_str(), (db_path + os_file_sep() + "index.lmdb-lock").c_str())!=0)
			{
				throw std::runtime_error("Error creating symlink at " + db_path + os_file_sep() + "index.lmdb-lock. " + folly::errnoStr(errno));
			}
		}
	}

	/*if (db_path.find("f0f42ce575714ecf80ec9324e59ef60a") != std::string::npos
		&& cache_path.empty())
	{
		Server->Log("Enabling debug freespace cache at /var/log/freespace_cache1", LL_WARNING);
		cache_path = "/var/log/freespace_cache1";
	}*/

	bool use_dm_cache = false;
	if (!options.dm_cache_path.empty()
		&& options.dm_cache_size > 20 * 1024 * 1024)
	{
		int64_t index_size = mapsize * 2;

		if (options.dm_cache_size > index_size)
			options.dm_cache_size = index_size;

		int64_t cache_meta_size = options.dm_cache_size / 100;

		cache_meta_size = ((cache_meta_size + 1 * 1024 * 1024) / (1 * 1024 * 1024)) * 1 * 1024 * 1024;
		options.dm_cache_size = ((options.dm_cache_size + 1 * 1024 * 1024) / (1 * 1024 * 1024)) * 1 * 1024 * 1024;

        std::error_code ec;
        std::filesystem::remove(options.dm_cache_path + "/cache", ec);
        int cache_fd = open((options.dm_cache_path + "/cache").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
		
        std::filesystem::remove(options.dm_cache_path + "/meta", ec);
        int cache_meta_fd = open((options.dm_cache_path + "/meta").c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);

		std::string cache_loop, meta_loop, db_loop;

        std::filesystem::path data_path_p(options.data_path);
        
		std::string cache_name = data_path_p.parent_path().filename().string() + "-" + random_uuid();

		{
            int fd = open(index_lmdb_fn.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
			if (fd!=-1)
			{
				int64_t fsize = fileSize(fd);
				if (fsize > 0
					&& fsize < index_size)
				{
                    folly::checkUnixError(ftruncate64(fd, index_size), "index_lmdb_fn ftrucate failed");
				}
			}
		}

		std::string db_direct_io = "--direct-io=on";
#ifndef _WIN32
		FILE* aFile;

		aFile = setmntent("/proc/mounts", "r");
		if (aFile != nullptr)
		{
			struct mntent* ent;
			std::string maxmount;
			std::string fsname;
			while (NULL != (ent = getmntent(aFile)))
			{
				if (next(db_path, 0, ent->mnt_dir)
					&& std::string(ent->mnt_dir).size() > maxmount.size())
				{
					maxmount = ent->mnt_dir;
					fsname = ent->mnt_fsname;
				}
			}
			endmntent(aFile);

			if (next(fsname, 0, "//"))
			{
				XLOG(WARN) << "Detected CIFS. Not using direct-io loop for cache";
				db_direct_io = "";
			}
		}
#endif

		std::string table_setup_cmd;
		if (cache_fd == -1)
		{
			XLOG(ERR) << "Error opening cache_f at " << options.dm_cache_path << "/cache";
		}
		else if (cache_meta_fd == -1)
		{
			XLOG(ERR) << "Error opening cache_meta_f at " << options.dm_cache_path << "/meta";
		}
		else if (fallocate64(cache_fd, 0, 0, options.dm_cache_size)!=0)
		{
            XLOG(ERR) << "Error resizing cache_f at " << options.dm_cache_path << "/cache " << folly::errnoStr(errno);
		}
		else if(fallocate64(cache_meta_fd, 0, 0, cache_meta_size)!=0)
		{
			XLOG(ERR) << "Error resizing cache_meta_f at " << options.dm_cache_path << "/meta " << folly::errnoStr(errno);
		}
		else if (os_popen("losetup -f --show \"" + db_path + os_file_sep() + "index.lmdb\" " + db_direct_io, db_loop) != 0)
		{
			XLOG(ERR) << "Error setting up db loop";
		}
		else if (os_popen("losetup -f --show \"" + options.dm_cache_path + "/cache\" --direct-io=on", cache_loop) != 0)
		{
            XLOG(ERR) << "Error setting up cache loop";
		}
		else if (os_popen("losetup -f --show \"" + options.dm_cache_path + "/meta\" --direct-io=on", meta_loop) != 0)
		{
            XLOG(ERR) << "Error setting up cache meta loop";
		}
		else if (!(table_setup_cmd = "dmsetup create \"" + cache_name + "\" --table '0 " + std::to_string(index_size / 512)
			+ " cache " + folly::trimWhitespace(meta_loop).toString() + " " + folly::trimWhitespace(cache_loop).toString() + " "
			+ folly::trimWhitespace(db_loop).toString() + " 2048 1 writethrough default 0'").empty()
			&& system(table_setup_cmd.c_str())!=0)
		{
			XLOG(ERR) << "Error setting up dm-cache (" << table_setup_cmd << ")";
		}
		else
		{
			index_lmdb_fn = "/dev/mapper/" + cache_name;
			use_dm_cache = true;

			cache_loop = getafter("/dev/", folly::trimWhitespace(cache_loop).toString());
			meta_loop = getafter("/dev/", folly::trimWhitespace(meta_loop).toString());

			//Disable flushing
            folly::writeFile(std::string("write through"), ("/sys/block/" + cache_loop + "/queue/write_cache").c_str());
            folly::writeFile(std::string("write through"), ("/sys/block/" + meta_loop + "/queue/write_cache").c_str());
		}
	}

	if (next(freespace_cache_path, 0, "nosync:"))
	{
		freespace_cache_path.erase(0, 7);
		sync_freespace_cache = false;
	}

#ifdef _WIN32
	use_direct_io = false;
#endif

	if (options.use_direct_io)
	{
        data_file_dio = MultiFile(data_file_path, data_file_chunk_size, O_RDONLY | O_CLOEXEC | O_DIRECT );
	}

	if (os_get_file_type(options.data_path + os_file_sep() + "new_data0")!=0)
	{
        new_data_file = MultiFile( options.data_path + os_file_sep() + "new_data", data_file_chunk_size,  O_RDWR | O_CLOEXEC);

		if (options.use_direct_io)
		{
            new_data_file_dio = MultiFile( options.data_path + os_file_sep() + "new_data", data_file_chunk_size, O_RDWR | O_CLOEXEC | O_DIRECT);
		}
	}

	if ( data_file.size() > 0
		&& index_file_size == 0)
	{
		throw std::runtime_error("Could not open index file at \"" + db_path + os_file_sep() + "index.lmdb");
	}

	int rc = mdb_env_create(&db_env);
	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to create LMDB env (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_maxreaders(db_env, 4094);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set max readers (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_mapsize(db_env, mapsize*2);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set map size (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_maxdbs(db_env, 8);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set max dbs (" + (std::string)mdb_strerror(rc) + ")");
	}

	if (mapsize + 2LL * 1024 * 1024 * 1024 > min_free_space)
	{
		min_free_space = mapsize + 2LL * 1024 * 1024 * 1024;
	}

	unsigned int mdb_flags = MDB_NOSUBDIR;

	with_rewrite = true;

    std::error_code ec;
	if ( (db_path != options.data_path
		|| use_dm_cache) 
		&& !std::filesystem::exists("/var/urbackup/sfs_with_readahead", ec) )
	{
		mdb_flags |= MDB_NORDAHEAD;
	}

	if (db_path != options.data_path
		|| std::filesystem::exists("/var/urbackup/sfs_no_rewrite", ec))
	{
		with_rewrite = false;
	}
	if (std::filesystem::exists("/var/urbackup/sfs_with_rewrite", ec))
	{
		with_rewrite = true;
	}

	if (std::filesystem::exists("/var/urbackup/sfs_use_lmdb_writemap", ec))
	{
		mdb_flags |= MDB_WRITEMAP;
	}

	rc = mdb_env_open(db_env, index_lmdb_fn.c_str(), mdb_flags, 0664);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to open LMDB database file (" + (std::string)mdb_strerror(rc) + ")");
	}
	
	THREAD_ID tid = gettid();
	setup_mmap_read_error(tid);

	MDB_txn* txn;
	rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for dbi handle (SIGBUS)");
	}

	int64_t current_txn_id = mdb_get_txnid(txn);

	mdb_txn_abort(txn);


	rc = mdb_txn_begin(db_env, NULL, 0, &txn);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(txn, "main", MDB_CREATE, &dbi_main);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening main dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening main dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(txn, "old", MDB_CREATE, &dbi_old);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening old dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening old dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(txn, "free", MDB_CREATE, &dbi_free);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening free dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening free dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting free comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	if(key_compare_func)
	{
		rc = mdb_set_compare(txn, dbi_main, key_compare_func);
		if (rc)
		{
			throw std::runtime_error("LMDB: Error setting main comparison function (" + (std::string)mdb_strerror(rc) + ")");
		}
	}

	rc = mdb_dbi_open(txn, "holes", MDB_CREATE, &dbi_holes);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening holes dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening holes dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, dbi_holes, mdb_cmp_varint);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting holes comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_dbi_open(txn, "size", MDB_CREATE, &dbi_size);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening size dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening size dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(txn, "free_len", MDB_CREATE|MDB_DUPSORT, &dbi_free_len);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening free_len dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening free_len dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting free_len comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_dbi_open(txn, "queue_del", MDB_CREATE, &dbi_queue_del);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening queue_del dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening queue_del dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(txn, "new_objects", MDB_CREATE|MDB_DUPSORT, &dbi_new_objects);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening new_objects dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening new_objects dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, dbi_new_objects, mdb_cmp_varint);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting new_objects comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	MDB_val val;
	char ch = dbi_size_info_size;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val size_out;

	rc = mdb_get(txn, dbi_size, &val, &size_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting data file max size (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting data file max size (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		if (size_out.mv_size == sizeof(data_file_offset) * 5 || 
			size_out.mv_size == sizeof(data_file_offset) * 6 ||
			size_out.mv_size == sizeof(data_file_offset) * 7 )
		{
			memcpy(&data_file_max_size, size_out.mv_data, sizeof(data_file_max_size));
			memcpy(&data_file_offset, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_max_size), sizeof(data_file_offset));
			memcpy(&data_file_offset_end, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_offset) * 2, sizeof(data_file_offset_end));
			memcpy(&data_file_free, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_offset) * 3, sizeof(data_file_free));
			memcpy(&curr_transid, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_offset) * 4, sizeof(curr_transid));
			if(size_out.mv_size > sizeof(data_file_offset) * 5)
				memcpy(&curr_partid, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_offset) * 5, sizeof(curr_partid));
			if(size_out.mv_size > sizeof(data_file_offset) * 6)
				memcpy(&curr_version, reinterpret_cast<char*>(size_out.mv_data) + sizeof(data_file_offset) * 6, sizeof(curr_version));
			XLOG(INFO) << "Data file max " << std::to_string(data_file_max_size) << " offset " << std::to_string(data_file_offset) <<
				" end " << std::to_string(data_file_offset_end) <<
				" free " << std::to_string(data_file_free) <<
				" transid " << std::to_string(curr_transid) << 
				" curr_partid " << curr_partid <<
				" curr_version " << curr_version << " fn " << data_file_path;
		}
		else
		{
			throw std::runtime_error("Size data has wrong size");
		}

		if (data_file_offset_end < 0)
		{
			data_file_max_size = 0;
			data_file_offset_end = -1;
			data_file_free = 0;
		}
	}
	else if (std::filesystem::exists(options.data_path + os_file_sep() + "active", ec))
	{
		throw std::runtime_error("Could not read data file information from index file \""
			+index_lmdb_fn + "\" "
			"(Index file size "+folly::prettyPrint(index_file_size,folly::PRETTY_BYTES_IEC)+")");
	}
	else
	{
		XLOG(INFO) << "New data file transid " << std::to_string(curr_transid) << " curr size "+folly::prettyPrint(index_file_size, folly::PRETTY_BYTES_IEC) << " fn " << data_file_path;
	}

	ch = dbi_size_info_next_disk_id;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val next_disk_id_out;

	rc = mdb_get(txn, dbi_size, &val, &next_disk_id_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting next disk id (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting next disk id (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		if (next_disk_id_out.mv_size == sizeof(next_disk_id))
		{
			memcpy(&next_disk_id, next_disk_id_out.mv_data, sizeof(next_disk_id));
			XLOG(INFO) << "Next disk id " << std::to_string(next_disk_id) << " fn " << data_file_path;
		}
	}

	ch = dbi_size_info_migration;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val copy_info_out;

	rc = mdb_get(txn, dbi_size, &val, &copy_info_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting datafile migration info (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting datafile migration info (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		if (copy_info_out.mv_size == sizeof(data_file_copy_done_sync))
		{
			memcpy(&data_file_copy_done, copy_info_out.mv_data, sizeof(data_file_copy_done));
			XLOG(INFO) << "data_file_copy_done=" << folly::prettyPrint(data_file_copy_done_sync, folly::PRETTY_BYTES_IEC);
			data_file_copy_max = data_file_copy_done_sync;
			data_file_copy_done = data_file_copy_done_sync;

			if (data_file_copy_done_sync > 0)
			{
				new_data_file = MultiFile(data_file_path.parent_path() / "new_data", data_file_chunk_size, O_RDWR | O_CLOEXEC);
				new_data_file_dio = MultiFile(data_file_path.parent_path() / "new_data", data_file_chunk_size, O_RDWR | O_CLOEXEC | O_DIRECT);
			}
		}
	}

	if (options.data_path != db_path)
	{
		XLOG(INFO) << "Data file metadata at " << index_lmdb_fn;
	}

	ch = dbi_size_info_enckey;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val enckey_info_out;

	rc = mdb_get(txn, dbi_size, &val, &enckey_info_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting enckey info (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting enckey info (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		if (enckey_info_out.mv_size == crypto_secretbox_KEYBYTES)
		{
			memcpy(enckey, enckey_info_out.mv_data, crypto_secretbox_KEYBYTES);
		}
		else
		{
			throw std::runtime_error("LMDB: Enckey has wrong size");
		}
	}
	else
	{
		crypto_secretbox_keygen(enckey);

		enckey_info_out.mv_data = enckey;
		enckey_info_out.mv_size = crypto_secretbox_KEYBYTES;

		rc = mdb_put(txn, dbi_size, &val, &enckey_info_out, 0);

		if (rc)
		{
			throw std::runtime_error("LMDB: Failed to put enckey into db on startup");
		}
		if (has_mmap_read_error_reset(tid))
		{
			throw std::runtime_error("LMDB: Failed to put enckey into db on startup (SIGBUS)");
		}
	}

	ch = dbi_size_info_ext_freespace;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val ext_freespace_info;

	rc = mdb_get(txn, dbi_size, &val, &ext_freespace_info);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting datafile ext freespace info (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting datafile ext freespace info (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		int64_t ext_freespace_info_val;
		if (ext_freespace_info.mv_size == sizeof(ext_freespace_info_val))
		{
			memcpy(&ext_freespace_info_val, ext_freespace_info.mv_data, sizeof(ext_freespace_info_val));
			if (ext_freespace_info_val & 1)
			{
				XLOG(INFO) << "Ext freespace sync";
			}
			else
			{
				XLOG(INFO) << "Ext freespace nosync";
				regen_freespace_cache = true;
			}

			if (freespace_cache_path.empty()
				|| freespace_cache_path == db_path)
			{
				regen_freespace_cache = true;
			}
		}
	}

	MDB_txn* freespace_txn = nullptr;

	if (!freespace_cache_path.empty()
		&& freespace_cache_path != db_path)
	{
		if (!regen_freespace_cache)
		{
			if (!open_cache_db(current_txn_id, mapsize, false, false, freespace_txn)
				&& !regen_freespace_cache)
			{
				if (!open_cache_db(current_txn_id, mapsize, true, false, freespace_txn))
				{
					regen_freespace_cache = true;
				}
			}
		}

		if (regen_freespace_cache)
		{
			bool b = open_cache_db(current_txn_id, mapsize, false, true, freespace_txn);
			if (!b)
			{
				abort();
			}
		}
	}

	if (data_file_free == 0 && freespace_txn!=nullptr && !regen_freespace_cache)
	{
		regen_datafile_free(freespace_txn);
	}

	XLOG(INFO) << "Free space in data file: " << folly::prettyPrint(data_file_free, folly::PRETTY_BYTES_IEC);

	if(freespace_txn!=nullptr && !regen_freespace_cache)
	{
		regen_free_len_idx(freespace_txn);
	}

	if(freespace_txn!=txn && freespace_txn!=nullptr)
	{
		rc = mdb_txn_commit(freespace_txn);
		if(rc)
		{
			throw std::runtime_error("LMDB: Error committing open_cache_db txn (" + (std::string)mdb_strerror(rc) + ")");
		}
		if (has_mmap_read_error_reset(tid))
		{
                	throw std::runtime_error("LMDB: Error committing open_cache_db txn (SIGBUS)");
		}
	}

	rc = mdb_txn_commit(txn);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error commiting txn for dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error commiting txn for dbi handle (SIGBUS)");
	}

	if (data_file_max_size < data_file.size())
	{
		XLOG(INFO) << "Trimming " << folly::prettyPrint(data_file.size() - data_file_max_size, folly::PRETTY_BYTES_IEC) << " from data file during recovery";
		data_file.truncate(data_file_max_size);
	}

	if (data_file_offset_end < 0)
	{
		data_file_offset_end = data_file_offset + alloc_chunk_size;
	}

	{
		std::scoped_lock lock(mmap_read_error_mutex);
		mmap_dbs.push_back(db_env);

		if (cache_db_env != nullptr)
			mmap_dbs.push_back(cache_db_env);
	}

	if (std::filesystem::exists(data_file_path.parent_path() / "new_data"))
	{
		migrate_thread_h = std::thread([this]() {
			auto cb = new SingleFileStorageMigrate(this);
			(*cb)();
		});
	}
}

SingleFileStorage::SingleFileStorage()
	: data_file_max_size(0), data_file_offset(0), data_file_offset_end(-1), data_file_free(0), 
	do_quit(false),	min_free_space(20LL * 1024 * 1024 * 1024), is_defragging(false), defrag_restart(0), db_path(std::string()),
	is_dead(true), write_offline(true), curr_transid(1), startup_finished(false),
	force_freespace_check(true), stop_defrag(false), allow_defrag(false), next_disk_id(1), data_file_copy_done(-1), data_file_copy_max(0), data_file_copy_done_sync(0),
	stop_data_file_copy(false), references(0),
	db_env(nullptr), cache_db_env(nullptr), regen_freespace_cache(false), sync_freespace_cache(true), key_compare_func(nullptr)
{

}

SingleFileStorage::~SingleFileStorage()
{
	if(commit_thread_h.joinable())
	{
		{
			std::scoped_lock lock(mutex);
			do_quit = true;
			cond.notify_all();
		}

		commit_thread_h.join();
	}

	if (migrate_thread_h.joinable())
	{
		{
			std::scoped_lock copy_lock(data_file_copy_mutex);
			stop_data_file_copy = true;
		}

		migrate_thread_h.join();
	}

	{
		std::scoped_lock lock(mmap_read_error_mutex);
		auto it = std::find(mmap_dbs.begin(), mmap_dbs.end(), db_env);
		if (it != mmap_dbs.end())
			mmap_dbs.erase(it);
	}

	for (uintptr_t addr : mmap_cleanup_addrs)
	{
		int page_size = getpagesize();
		if (page_size <= 0)
			page_size = 4096;

#ifndef _WIN32
		if (munmap(reinterpret_cast<void*>(addr), page_size) != 0)
		{
			XLOG(ERR) << "Munmap failed. addr="
				<< std::to_string(static_cast<int64_t>(addr))
				<< " page_size=" << std::to_string(page_size)
				<< " " << folly::errnoStr(errno);
			abort();
		}
#endif
	}

	/*if (is_dead)
		mdb_env_unmap(db_env);*/

	mdb_env_close(db_env);
}

void SingleFileStorage::init_mutex()
{
#ifndef _WIN32
	struct sigaction act;
	act.sa_sigaction = mmap_read_error;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO| SA_RESTART;
	sigaction(SIGBUS, &act, NULL);
#endif
}

void SingleFileStorage::handle_mmap_read_error(void* addr)
{
	std::scoped_lock lock(mmap_read_error_mutex);
	auto it = mmap_read_error_jmp.find(gettid());
	if (it != mmap_read_error_jmp.end())
	{
		int page_size = getpagesize();
		if (page_size <= 0)
			page_size = 4096;

		uintptr_t addr_page = reinterpret_cast<uintptr_t>(addr);
		if (addr_page % page_size != 0)
		{
			addr_page = (addr_page / page_size)*page_size;
		}

		bool found = false;
		for (MDB_env* env : mmap_dbs)
		{
			char* map_ptr;
			size_t map_size;
			mdb_get_map(env, &map_ptr, &map_size);

			uintptr_t map_ptr_addr = reinterpret_cast<uintptr_t>(map_ptr);

			if (addr_page >= map_ptr_addr
				&& addr_page < map_ptr_addr + map_size
				&& addr_page+page_size >= map_ptr_addr
				&& addr_page+page_size <= map_ptr_addr+map_size)
			{
				found = true;
				const char* env_path;
				mdb_env_get_path(env, &env_path);
				XLOG(WARN) << std::string("SIGBUS occured in ") << env_path;
				break;
			}
		}

		if (!found)
		{
			XLOG(ERR) << "Did not find addr=" << std::to_string(reinterpret_cast<int64_t>(addr))
				<< " page=" << std::to_string(static_cast<int64_t>(addr_page)) << " in any LMDB mapping";
			for (MDB_env* env : mmap_dbs)
			{
				char* map_ptr;
				size_t map_size;
				mdb_get_map(env, &map_ptr, &map_size);
				const char* env_path;
				mdb_env_get_path(env, &env_path);
				XLOG(ERR) << "Mapping: " << std::to_string((int64_t)map_ptr) << " size " << std::to_string(map_size) << " (" << env_path << ")";
			}
			abort();
		}

#ifndef _WIN32
		if (mmap(reinterpret_cast<void*>(addr_page), page_size, PROT_READ|PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
			-1, 0) == MAP_FAILED)
#endif
		{
			XLOG(ERR) << "Failed mmap in SIGBUS signal handler. addr="
				<< std::to_string(reinterpret_cast<int64_t>(addr))
				+" addr_page="+std::to_string(static_cast<int64_t>(addr_page))
				+ " "+folly::errnoStr(errno);
			abort();
		}
#ifndef _WIN32
		else
#endif
		{
			XLOG(WARN) << "SIGBUS at addr=" << std::to_string(reinterpret_cast<int64_t>(addr)) 
				+ ". Mapping " + std::to_string(page_size) + " zeroes at addr " + std::to_string(static_cast<int64_t>(addr_page));
		}
		it->second.first = true;
		it->second.second.push_back(addr_page);
	}
	else
	{
		XLOG(ERR) << "Not setup to catch error in SIGBUS signal handler";
		abort();
	}
}

int SingleFileStorage::write(const std::string & fn, const char* data, 
	size_t data_size, const size_t data_alloc_size, int64_t last_modified, const std::string & md5sum, const std::string& metadata,
	bool no_del_old, bool is_fragment)
{
	if (is_dead)
	{
		return ENODEV;
	}

	if (fn.size() > 255)
		return EINVAL;

	return write_int(fn, data, data_size, data_alloc_size, last_modified, md5sum, metadata, true, no_del_old);
}

int64_t SingleFileStorage::get_transid(int64_t disk_id)
{
	std::scoped_lock lock(mutex);

	THREAD_ID tid = gettid();

	setup_mmap_read_error(tid);

	MDB_txn* txn;
	int rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for get_transid (" << mdb_strerror(rc) << ") txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
		clear_mmap_read_error(tid);
		return -1;
	}

	int64_t ret = get_disk_trans_id(txn, tid, disk_id);

	mdb_txn_abort(txn);

	return ret;
}

std::string SingleFileStorage::meminfo()
{
	std::string ret = "##SingleFileStorage:\n";
	{
		std::scoped_lock lock(mutex);
		ret += "  defrag_skip_items: " + std::to_string(defrag_skip_items.size()) + " * " + folly::prettyPrint(sizeof(std::string), folly::PRETTY_BYTES_IEC) + "\n";
		ret += "  commit_queue: " + std::to_string(commit_queue.size()) + " * " + folly::prettyPrint(sizeof(SFragInfo), folly::PRETTY_BYTES_IEC) + "\n";
		ret += "  commit_background_queue: " + std::to_string(commit_background_queue.size()) + " * " + folly::prettyPrint(sizeof(SFragInfo), folly::PRETTY_BYTES_IEC) + "\n";
		ret += "  defrag_items: " + std::to_string(defrag_items.size()) + " * " + folly::prettyPrint(sizeof(std::string), folly::PRETTY_BYTES_IEC) + "\n";
	}
	{
		std::scoped_lock lock(mmap_read_error_mutex);
		ret += "  mmap_read_error_jmp: " + std::to_string(mmap_read_error_jmp.size()) + " * " + folly::prettyPrint(sizeof(THREAD_ID)+sizeof(bool), folly::PRETTY_BYTES_IEC) + "\n";
	}
	return ret;
}

bool SingleFileStorage::set_write_offline(bool b)
{
	XLOG(ERR) << "SFS " << db_path << " set write offline b=" << std::to_string(b);

	if (!b && write_offline)
	{
		write_offline = b;
		do_stop_on_error();

		std::unique_lock lock(mutex);

		SCommitInfo commit_info;
		SFragInfo frag_info;
		frag_info.md5sum = "reset";
		frag_info.action = FragAction::Commit;
		frag_info.commit_info = &commit_info;

		if (is_dead)
			return false;

		commit_queue.push_back(frag_info);

		cond.notify_all();
		commit_info.commit_done.wait(lock);

		return commit_info.commit_errors == 0;
	}

	write_offline = b;

	do_stop_on_error();
	return true;
}

bool SingleFileStorage::set_allow_defrag(bool b, int64_t disk_id)
{
	std::scoped_lock lock(mutex);

	if (!b)
	{
		disallow_defrag_disk_id.insert(disk_id);
	}
	else
	{
		disallow_defrag_disk_id.erase(disk_id);
	}

	allow_defrag = disallow_defrag_disk_id.empty();

	return true;
}

bool SingleFileStorage::reset_del_log(int64_t disk_id, int64_t reset_transid)
{
	if (is_dead)
	{
		return false;
	}

	std::unique_lock lock(mutex);

	SCommitInfo commit_info;
	SFragInfo frag_info;
	frag_info.offset = reset_transid;
	frag_info.last_modified = disk_id;
	frag_info.action = FragAction::ResetDelLog;
	frag_info.commit_info = &commit_info;

	commit_queue.push_back(frag_info);

	cond.notify_all();
	commit_info.commit_done.wait(lock);

	return commit_info.commit_errors == 0;
}

bool SingleFileStorage::reset_del_queue(int64_t disk_id, int64_t reset_transid)
{
	if (is_dead)
	{
		return false;
	}

	std::unique_lock lock(mutex);

	SCommitInfo commit_info;
	SFragInfo frag_info;
	frag_info.offset = reset_transid;
	frag_info.last_modified = disk_id;
	frag_info.action = FragAction::ResetDelQueue;
	frag_info.commit_info = &commit_info;

	commit_queue.push_back(frag_info);

	cond.notify_all();
	commit_info.commit_done.wait(lock);

	return commit_info.commit_errors == 0;
}

int64_t SingleFileStorage::get_disk_id(const std::string & uuid)
{
	if (is_dead)
	{
		return false;
	}

	std::unique_lock lock(mutex);

	SCommitInfo commit_info;
	SFragInfo frag_info;
	frag_info.fn = uuid;
	frag_info.action = FragAction::GetDiskId;
	frag_info.commit_info = &commit_info;

	commit_queue.push_back(frag_info);

	cond.notify_all();
	commit_info.commit_done.wait(lock);

	if (commit_info.commit_errors != 0)
		return 0;

	return commit_info.new_datafile_offset;
}

void SingleFileStorage::migrate_thread()
{
	std::unique_lock lock(data_file_copy_mutex);

	auto new_data_file_path = data_file_path.parent_path() / "new_data";

	if (!new_data_file)
	{
		new_data_file = MultiFile(new_data_file_path, data_file_chunk_size, O_RDWR | O_CLOEXEC);

		if (!data_file_dio)
		{
			new_data_file_dio = MultiFile(new_data_file_path, data_file_chunk_size, O_RDWR | O_CLOEXEC | O_DIRECT);
		}
	}

	auto status_fn = data_file_path.parent_path() / "new_data.status";

	std::vector<char> buf;
	buf.resize(512 * 1024);
	int64_t pos = (std::max)((int64_t)0, data_file_copy_done);
	int64_t data_file_size;
	while (pos < (data_file_size=data_file.size())
		&& !stop_data_file_copy)
	{
		data_file_copy_max = (std::min)(pos + data_file_copy_num_bytes, data_file_size);

		lock.unlock();

		while (pos < data_file_copy_max
			&& !stop_data_file_copy)
		{
			int64_t tocopy = (std::min)(static_cast<int64_t>(buf.size()), data_file_copy_max - pos);

			unsigned int read;
			if ( (read=data_file.preadFull(buf.data(), static_cast<size_t>(tocopy), pos)) != tocopy)
			{
				if (errno == EIO)
				{
					XLOG(ERR) << "Error reading " << std::to_string(tocopy) << " bytes at pos " << std::to_string(pos) << " from " << data_file_path << " for migration. " << folly::errnoStr(errno) << ". Ignoring read error, writing zeroes...";
					memset(buf.data() + read, 0, tocopy - read);
				}
				else
				{

					XLOG(ERR) << "Error reading " << std::to_string(tocopy) << " bytes at pos " << std::to_string(pos) << " from " << data_file_path << " for migration. " << folly::errnoStr(errno);
					folly::writeFileAtomic(status_fn.string(), "{\"status\": \"error\"}");
					return;
				}
			}

			if (new_data_file.pwriteFull(buf.data(), static_cast<size_t>(tocopy), pos)!=tocopy)
			{
				XLOG(ERR) << "Error writing " << std::to_string(tocopy) << " bytes at pos " << std::to_string(pos) << " to " << new_data_file_path << " for migration. " << folly::errnoStr(errno);
				folly::writeFileAtomic(status_fn.string(), "{\"status\": \"error\"}");
				return;
			}

			pos += tocopy;
		}

		if (new_data_file.fsyncNoInt()!=0)
		{
			XLOG(ERR) << "Error syncing " << new_data_file_path << " (pos "<<std::to_string(data_file_copy_done)<<" to "<<std::to_string(data_file_copy_max)<<") for migration. " << folly::errnoStr(errno);
			folly::writeFileAtomic(status_fn.string(), "{\"status\": \"error\"}");
			return;
		}

		int complete_pc = static_cast<int>((pos * 100) / data_file_size);
		folly::writeFileAtomic(status_fn.string(), "{\"status\": \"running\", \"complete\": "+std::to_string(complete_pc)+"}");

		lock.lock();
		data_file_copy_done = pos;
	}

	if (pos == data_file.size())
	{
		data_file_copy_done = LLONG_MAX;
		data_file_copy_max = LLONG_MAX;

		folly::writeFileAtomic(status_fn.string(), "{\"status\": \"done\"}");
	}
}

bool SingleFileStorage::start_migrate()
{
	if (is_dead)
	{
		return false;
	}

	std::scoped_lock copy_lock(data_file_copy_mutex);

	if (migrate_thread_h.joinable())
		return false;

	if (!std::filesystem::exists(data_file_path.parent_path() / "new_data"))
		return false;

	auto status_fn = data_file_path.parent_path()/ "new_data.status";
	std::filesystem::remove(status_fn);
	folly::writeFileAtomic(status_fn.string(), "{\"status\": \"init\"}");

	migrate_thread_h = std::thread([this](){
		auto cb = new SingleFileStorageMigrate(this);
		(*cb)();
	});
	
	return true;
}

void SingleFileStorage::reference()
{
	++references;
}

void SingleFileStorage::unreference()
{
	--references;
}

SingleFileStorage::WritePrepareResult SingleFileStorage::write_prepare(const std::string& fn, size_t data_size)
{
	if (is_dead)
	{
		return WritePrepareResult{ENODEV};
	}

	if (fn.size() > 255)
		return WritePrepareResult{EINVAL};

	if(data_size==0)
	{
		return WritePrepareResult{0, {}};
	}

	std::vector<Ext> extents;
	size_t data_size_remaining = data_size;
	{
		std::lock_guard lock(datafileoffset_mutex);

		while (data_size_remaining > 0)
		{
			Ext curr_ext;

			if (data_file_offset_end > 0
				&& data_file_offset >= data_file_offset_end)
			{
				XLOG(DBG) << "Current data offset " << std::to_string(data_file_offset) << " out of extent end (" << std::to_string(data_file_offset_end) << "). Searching for new free extent...";

				std::unique_lock lock(mutex);
				SCommitInfo commit_info;
				SFragInfo frag_info;
				frag_info.action = FragAction::FindFree;
				frag_info.commit_info = &commit_info;
				frag_info.offset = data_file_offset_end;

				if (is_dead)
					return WritePrepareResult{EIO};

				commit_queue.push_back(frag_info);

				cond.notify_all();
				commit_info.commit_done.wait(lock);

				if (commit_info.commit_errors != 0)
				{
					lock.unlock();
					free_extents(extents);
					if (commit_info.commit_errors == LLONG_MAX)
					{
						return WritePrepareResult{ENOSPC};
					}
					return WritePrepareResult{EIO};
				}

				data_file_offset = commit_info.new_datafile_offset;
				data_file_offset_end = commit_info.new_datafile_offset_end;

				XLOG(INFO) << "Found new free extent (" << std::to_string(data_file_offset) << ", " << std::to_string(data_file_offset_end) << "). Size " << folly::prettyPrint(data_file_offset_end - data_file_offset, folly::PRETTY_BYTES_IEC);
			}

			if (data_file_offset_end > data_file_offset)
			{
				int64_t remaining = data_file_offset_end - data_file_offset;
				assert(remaining >= block_size);
				curr_ext.data_file_offset = data_file_offset;
				curr_ext.len = (std::min)(static_cast<int64_t>(data_size_remaining), remaining);
			}
			else
			{
				curr_ext.data_file_offset = data_file_offset;
				curr_ext.len = data_size_remaining;
			}

			data_file_offset += div_up(curr_ext.len, block_size)*block_size;
			curr_ext.obj_offset = data_size - data_size_remaining;
			data_size_remaining -= curr_ext.len;

			{
				std::scoped_lock lock(reserved_extents_mutex);
				reserved_extents[curr_ext.data_file_offset] = curr_ext.len;
			}

			extents.push_back(curr_ext);
		}
	}

	return WritePrepareResult{0, extents};
}

int SingleFileStorage::write_ext(const Ext& ext, const char* data, size_t data_size)
{
	std::shared_lock copy_lock(data_file_copy_mutex);

	auto sel_data_file = &data_file;

	while (data_file_copy_done!=-1
		&& (ext.data_file_offset >= data_file_copy_done
		&& ext.data_file_offset <= data_file_copy_max) 
		|| (ext.data_file_offset+ext.len >= data_file_copy_done
			&& ext.data_file_offset+ext.len <= data_file_copy_max) )
	{
		copy_lock.unlock();
		std::this_thread::sleep_for(1s);
		copy_lock.lock();
	}

	if (ext.data_file_offset + ext.len >= data_file_copy_done_sync
		&& ext.data_file_offset + ext.len <= data_file_copy_done)
	{
		if (sel_data_file->pwriteFullFillPage(data, data_size, ext.data_file_offset) != data_size)
		{
			std::string fn =
				sel_data_file == &data_file ? data_file_path : (data_file_path.parent_path() / "new_data");

			XLOG(ERR) << "Error writing to data file " << fn << ". " << folly::errnoStr(errno);

			if (errno > 0)
				return errno;
			else
				return EIO;
		}
	}

	if (ext.data_file_offset+ext.len <= data_file_copy_done)
	{
		sel_data_file = &new_data_file;
	}

	EXT_DEBUG(XLOG(INFO) << "Writing " << data_file_path << " to offset " << std::to_string(ext.data_file_offset) << " len " << std::to_string(data_size));

	if (sel_data_file->pwriteFullFillPage(data, data_size, ext.data_file_offset) != data_size )
	{
		std::string fn =
				sel_data_file == &data_file ? data_file_path : (data_file_path.parent_path() / "new_data");

		XLOG(ERR) << "Error writing to data file " << fn + ". " + folly::errnoStr(errno);

		if (errno > 0)
			return errno;
		else
			return EIO;
	}

	return 0;
}

int SingleFileStorage::write_finalize(const std::string& fn, const std::vector<Ext>& extents, int64_t last_modified, const std::string& md5sum,
		const std::string& metadata, bool no_del_old, bool is_fragment)
{
	std::unique_lock lock(mutex);
	wait_queue(lock, false, true);
	wait_defrag(fn, lock);

	SFragInfo curr_frag(extents[0].data_file_offset, extents[0].len);
	for (size_t i = 1; i < extents.size(); ++i)
	{
		assert(extents[0].len>0);
		assert(extents[i].len>0);
		curr_frag.extra_exts.push_back(SPunchItem(extents[i].data_file_offset, extents[i].len));
	}
	curr_frag.action = no_del_old ? FragAction::AddNoDelOld : FragAction::Add;
	curr_frag.fn = fn;
	curr_frag.last_modified = last_modified;
	curr_frag.md5sum = md5sum;
	curr_frag.metadata = metadata;

	++commit_items[common_prefix_hash_func(fn)];

	commit_queue.push_back(curr_frag);
	
	if (is_defragging)
	{
		defrag_skip_items.insert(fn);
	}
	cond.notify_all();

	return 0;
}

int SingleFileStorage::write_int(const std::string & fn, const char* data,
	size_t data_size, const size_t data_alloc_size, int64_t last_modified, const std::string & md5sum, const std::string& metadata, bool allow_defrag_lock,
	bool no_del_old)
{
	std::string cfn = fn;

	std::vector<Ext> extents;
	size_t data_size_remaining = data_size;
	if(data_size_remaining>0)
	{
		if (!allow_defrag_lock)
		{
			if (!datafileoffset_mutex.try_lock())
			{
				return EDEADLK;
			}
		}
		else
		{
			datafileoffset_mutex.lock();
		}

		SCOPE_EXIT { datafileoffset_mutex.unlock(); };

		while (data_size_remaining > 0)
		{
			Ext curr_ext;

			if (data_file_offset_end > 0
				&& data_file_offset >= data_file_offset_end)
			{
				XLOG(DBG) << "Current data offset " << std::to_string(data_file_offset) << " out of extent end (" << std::to_string(data_file_offset_end) << "). Searching for new free extent...";

				std::unique_lock lock(mutex);
				SCommitInfo commit_info;
				SFragInfo frag_info;
				frag_info.action = FragAction::FindFree;
				frag_info.commit_info = &commit_info;
				frag_info.offset = data_file_offset_end;
				if (!allow_defrag_lock)
				{
					//write in defrag
					frag_info.len = 1;
				}

				if (is_dead)
					return EIO;

				commit_queue.push_back(frag_info);

				cond.notify_all();
				commit_info.commit_done.wait(lock);

				if (commit_info.commit_errors != 0)
				{
					lock.unlock();
					free_extents(extents);
					if (commit_info.commit_errors == LLONG_MAX)
					{
						return ENOSPC;
					}
					return EIO;
				}

				data_file_offset = commit_info.new_datafile_offset;
				data_file_offset_end = commit_info.new_datafile_offset_end;

				XLOG(INFO) << "Found new free extent (" << std::to_string(data_file_offset) << ", " << std::to_string(data_file_offset_end) << "). Size " << folly::prettyPrint(data_file_offset_end - data_file_offset, folly::PRETTY_BYTES_IEC);
			}

			if (data_file_offset_end > data_file_offset)
			{
				int64_t remaining = data_file_offset_end - data_file_offset;
				assert(remaining >= block_size);
				curr_ext.data_file_offset = data_file_offset;
				curr_ext.len = (std::min)(static_cast<int64_t>(data_size_remaining), remaining);
			}
			else
			{
				curr_ext.data_file_offset = data_file_offset;
				curr_ext.len = data_size_remaining;
			}

			data_file_offset += div_up(curr_ext.len, block_size)*block_size;
			curr_ext.obj_offset = data_size - data_size_remaining;
			data_size_remaining -= curr_ext.len;

			extents.push_back(curr_ext);
		}
	}

	if(!extents.empty())
	{
		std::shared_lock copy_lock(data_file_copy_mutex);

		size_t data_offset = 0;
		for (Ext& ext : extents)
		{
			int64_t resv_len = div_up(ext.len, block_size) * block_size;
			if (data_offset + resv_len > data_alloc_size)
				resv_len = ext.len;
			
			auto sel_data_file = &data_file;

			while (data_file_copy_done!=-1
				&& (ext.data_file_offset >= data_file_copy_done
				&& ext.data_file_offset <= data_file_copy_max) 
				|| (ext.data_file_offset+resv_len >= data_file_copy_done
					&& ext.data_file_offset+resv_len <= data_file_copy_max) )
			{
				copy_lock.unlock();
				std::this_thread::sleep_for(1s);
				copy_lock.lock();
			}

			if (ext.data_file_offset + resv_len >= data_file_copy_done_sync
				&& ext.data_file_offset + resv_len <= data_file_copy_done)
			{
				if (sel_data_file->pwriteFull(data + data_offset, static_cast<size_t>(resv_len), ext.data_file_offset) != static_cast<ssize_t>(resv_len))
				{
					std::string fn =
						sel_data_file == &data_file ? data_file_path : (data_file_path.parent_path() / "new_data");

					XLOG(ERR) << "Error writing to data file " << fn << ". " << folly::errnoStr(errno);
					free_extents(extents);
					if (errno > 0)
						return errno;
					else
						return EIO;
				}
			}

			if (ext.data_file_offset+resv_len <= data_file_copy_done)
			{
				sel_data_file = &new_data_file;
			}

			EXT_DEBUG(XLOG(INFO) << "Writing " << fn << " to offset " << std::to_string(ext.offset) << " len " << std::to_string(ext.len) << " resv len " << std::to_string(resv_len) )

			if (sel_data_file->pwriteFull(data + data_offset, static_cast<size_t>(resv_len), ext.data_file_offset) != static_cast<ssize_t>(resv_len))
			{
				std::string fn =
						sel_data_file == &data_file ? data_file_path : (data_file_path.parent_path() / "new_data");

				XLOG(ERR) << "Error writing to data file " << fn + ". " + folly::errnoStr(errno);
				free_extents(extents);
				if (errno > 0)
					return errno;
				else
					return EIO;
			}
			data_offset += ext.len;
		}
		assert(data_offset == data_size);

	}

	if (extents.size() > 1)
	{
		XLOG(INFO) << "Item " << fn << " has " << std::to_string(extents.size()) << " extents";
	}
	else if(extents.empty())
	{
		extents.push_back(Ext(0, 0, 0));
	}

	std::unique_lock lock(mutex);
	wait_queue(lock, false, !allow_defrag_lock);
	if (allow_defrag_lock)
	{
		wait_defrag(cfn, lock);
	}

	SFragInfo curr_frag(extents[0].data_file_offset, extents[0].len);
	for (size_t i = 1; i < extents.size(); ++i)
	{
		curr_frag.extra_exts.push_back(SPunchItem(extents[i].data_file_offset, extents[i].len));
	}
	curr_frag.action = no_del_old ? FragAction::AddNoDelOld : FragAction::Add;
	curr_frag.fn = cfn;
	curr_frag.last_modified = last_modified;
	curr_frag.md5sum = md5sum;
	curr_frag.metadata = metadata;

	++commit_items[common_prefix_hash_func(cfn)];

	commit_queue.push_back(curr_frag);
	
	if (is_defragging)
	{
		defrag_skip_items.insert(cfn);
	}
	cond.notify_all();

	return 0;
}

void SingleFileStorage::add_reading_item(const SingleFileStorage::SFragInfo& fi)
{
	auto& ri = reading_items[fi.offset];
	++ri.refs;
}

void SingleFileStorage::remove_reading_item(const std::vector<Ext>& extents)
{
	assert(!extents.empty());
	auto ri = reading_items.find(extents.begin()->data_file_offset);
	assert(ri!=reading_items.end());
	--ri->second.refs;
	assert(ri->second.refs>=0);
	if(ri->second.refs<=0)
	{
		if(ri->second.free_skip)
		{
			for(const auto& ext : extents)
			{
				auto it = reading_free_skip_extents.find(ext.data_file_offset);
				if(it!=reading_free_skip_extents.end())
					reading_free_skip_extents.erase(it);
			}
		}

		reading_items.erase(ri);
	}
}

SingleFileStorage::ReadPrepareResult SingleFileStorage::read_prepare(const std::string_view fn, unsigned int flags)
{
	if (is_dead)
	{
		return ReadPrepareResult{ENOTRECOVERABLE};
	}

	if ((flags & ReadUnsynced) == 0)
	{
		std::unique_lock lock(mutex);

		auto it = commit_items.find(common_prefix_hash_func(fn));
		if(it!=commit_items.end())
		{
			flags |= ReadUnsynced;
		}
	}

	const bool read_newest = (flags & ReadNewest);

	SFragInfo frag_info;
	if ((flags & ReadUnsynced) == 0)
	{
		frag_info = get_frag_info(nullptr, fn, true, read_newest);

		if (frag_info.offset == -1)
		{
			XLOG(INFO) << "Could not find metadata for fragment " << fn << " in LMDB sfs " << db_path;
			return ReadPrepareResult{ENOENT};
		}

		if(!(flags & ReadSkipAddReading))
		{
			std::lock_guard lock(mutex);
			add_reading_item(frag_info);
		}
	}
	else
	{
		SCommitInfo commit_info;
		commit_info.frag_info = &frag_info;
		SFragInfo curr_frag;
		curr_frag.action = FragAction::ReadFragInfo;
		curr_frag.fn = fn;
		curr_frag.commit_info = &commit_info;
		curr_frag.offset = read_newest ? 1 : 0;

		std::unique_lock lock(mutex);
		wait_startup_finished(lock);

		commit_queue.push_back(curr_frag);
		cond.notify_all();
		commit_info.commit_done.wait(lock);

		if (frag_info.offset == -1)
		{
			XLOG(INFO) << "Could not find metadata for fragment " << fn << " in LMDB (read unsynced) sfs " << db_path;
			return ReadPrepareResult{ENOENT};
		}

		if(!(flags & ReadSkipAddReading))
			add_reading_item(frag_info);
	}

	ReadPrepareResult res = {0};
	res.extents.reserve(1 + frag_info.extra_exts.size());
	res.extents.push_back(Ext(0, frag_info.offset, frag_info.len));
	res.total_len += frag_info.len;
	for (const SPunchItem& ext : frag_info.extra_exts)
	{
		res.extents.push_back(Ext(res.total_len, ext.offset, ext.len));
		res.total_len += ext.len;
	}
	res.md5sum = frag_info.md5sum;

	if(read_newest)
	{
		res.key = frag_info.fn;
	}

	return res;	
}

int SingleFileStorage::check_existence(const std::string_view fn, unsigned int flags)
{
	if (is_dead)
	{
		return ENOTRECOVERABLE;
	}

	if ((flags & ReadUnsynced) == 0)
	{
		std::unique_lock lock(mutex);

		auto it = commit_items.find(common_prefix_hash_func(fn));
		if(it!=commit_items.end())
		{
			flags |= ReadUnsynced;
		}
	}

	const bool read_newest = (flags & ReadNewest);

	if ((flags & ReadUnsynced) == 0)
	{
		auto frag_info = get_frag_info(nullptr, fn, false, read_newest);

		if (frag_info.offset == -1)
		{
			XLOG(INFO) << "Could not find metadata for fragment " << fn << " in LMDB sfs " << db_path;
			return ENOENT;
		}
	}
	else
	{
		SFragInfo frag_info;
		SCommitInfo commit_info;
		commit_info.frag_info = &frag_info;
		SFragInfo curr_frag;
		curr_frag.action =FragAction::ReadFragInfoWithoutParsing;
		curr_frag.fn = fn;
		curr_frag.commit_info = &commit_info;
		curr_frag.offset = read_newest ? 1 : 0;

		std::unique_lock lock(mutex);
		wait_startup_finished(lock);

		commit_queue.push_back(curr_frag);
		cond.notify_all();
		commit_info.commit_done.wait(lock);

		if (frag_info.offset == -1)
		{
			XLOG(INFO) << "Could not find metadata for fragment " << fn << " in LMDB (read unsynced) sfs " << db_path;
			return ENOENT;
		}
	}

	return 0;
}

std::string SingleFileStorage::read(const std::string& fn, const unsigned int flags)
{
	auto prepRes = read_prepare(fn, flags);
	if(prepRes.err!=0)
		return std::string();

	std::string data;
	folly::IOBufQueue buf;
	for(const auto& ext: prepRes.extents)
	{
		auto readExtRes = read_ext(ext, 0, ext.len, buf);
		if(readExtRes.err!=0)
		{
			read_finalize(fn, prepRes.extents, flags);
			return std::string();
		}

		data.append(reinterpret_cast<const char*>(readExtRes.buf->buffer()), readExtRes.buf->length());
	}

	return data;
}


SingleFileStorage::ReadExtResult SingleFileStorage::read_ext(const Ext& ext, const unsigned int flags, const size_t bufsize, folly::IOBufQueue& buf)
{
	if (is_dead)
	{
		return ReadExtResult{ENOTRECOVERABLE};
	}

	std::shared_lock copy_lock(data_file_copy_mutex);

	auto sel_data_file = &data_file;
	auto sel_data_file_dio = &data_file_dio;

	size_t toread = std::min(static_cast<size_t>(ext.len), bufsize);

	if (ext.data_file_offset+static_cast<int64_t>(toread) <= data_file_copy_done)
	{
		sel_data_file = &new_data_file;
		sel_data_file_dio = &new_data_file_dio;
	}

	auto data = buf.preallocate(bufsize, bufsize);
	auto bufptr = reinterpret_cast<char*>(data.first);

	ssize_t read;
	bool dio_read = (flags & ReadWithReadahead) == 0 && (flags & ReadUnsynced) == 0 && *sel_data_file_dio;
	if (dio_read)
	{
		read = sel_data_file_dio->preadFull(bufptr, toread, ext.data_file_offset);
	}
	else
	{
		read = sel_data_file->preadFull(bufptr, toread, ext.data_file_offset);
	}

	if (read<ssize_t(toread))
	{
		if (dio_read)
		{
			read = sel_data_file->preadFull(bufptr, toread, ext.data_file_offset);
		}
		if (read < ssize_t(toread))
		{
			return ReadExtResult{errno};
		}
		else
		{
			XLOG(WARN) << "Read succeeded with non-dio";
		}
	}

	buf.postallocate(read);
	
	return ReadExtResult{0, buf.move()};
}

int SingleFileStorage::read_finalize(const std::string& fn, const std::vector<Ext>& extents, unsigned int flags)
{
	if(!extents.empty())
	{
		std::lock_guard lock(mutex);
		remove_reading_item(extents);
	}
	return 0;
}

int SingleFileStorage::del(const std::string_view fn, DelAction da,
	bool background_queue)
{
	if (is_dead)
	{
		return ENOTRECOVERABLE;
	}

	int rc = check_existence(fn, 0);
	if(rc)
		return rc;

	SFragInfo curr_frag;
	switch (da)
	{
	case DelAction::Del:
		curr_frag.action = FragAction::Del;
		break;
	case DelAction::DelOld:
		curr_frag.action = FragAction::DelOld;
		break;
	case DelAction::DelWithQueued:
		curr_frag.action = FragAction::DelWithQueued;
		break;
	case DelAction::Queue:
		curr_frag.action = FragAction::QueueDel;
		break;
	case DelAction::Unqueue:
		curr_frag.action = FragAction::UnqueueDel;
		break;
	case DelAction::AssertQueueEmpty:
		curr_frag.action = FragAction::AssertDelQueueEmpty;
		break;
	}
	curr_frag.fn = fn;

	std::unique_lock lock(mutex);
	wait_queue(lock, background_queue, false);
	wait_defrag(fn, lock);
	++commit_items[common_prefix_hash_func(fn)];
	
	if (is_defragging)
	{
		defrag_skip_items.insert(std::string(fn));
	}
	if (background_queue)
	{
		commit_background_queue.push_back(curr_frag);
	}
	else
	{
		commit_queue.push_back(curr_frag);
	}
	cond.notify_all();

	return 0;
}

bool SingleFileStorage::restore_old(const std::string & fn)
{
	if (is_dead)
	{
		return false;
	}

	std::string cfn = compress_filename(fn);

	SFragInfo curr_frag;
	curr_frag.action = FragAction::RestoreOld;
	curr_frag.fn = cfn;

	std::unique_lock lock(mutex);
	wait_queue(lock, false, false);
	wait_defrag(cfn, lock);
	++commit_items[common_prefix_hash_func(cfn)];

	if (is_defragging)
	{
		defrag_skip_items.insert(cfn);
	}
	commit_queue.push_back(curr_frag);
	cond.notify_all();

	return true;
}

bool SingleFileStorage::commit(bool background_queue, int64_t transid, int64_t disk_id)
{
	if (is_dead)
	{
		return false;
	}

	if (data_file.fsyncNoInt()!=0)
	{
		XLOG(ERR) << "Failed to sync data file " << data_file_path << ". " << folly::errnoStr(errno);
		write_offline = true;
		do_stop_on_error();
		return false;
	}

	{
		std::scoped_lock lock(mutex);
		mdb_curr_sync = true;
	}

	if (mdb_env_sync(db_env, 0) != 0)
	{
		XLOG(ERR) << "mdb_env_sync on " << db_path << " failed. " << folly::errnoStr(errno);
		write_offline = true;
		do_stop_on_error();
		return false;
	}

	if (cache_db_env != nullptr)
	{
		if (mdb_env_sync(cache_db_env, 0) != 0)
		{
			XLOG(ERR) << "mdb_env_sync on cache db failed. " << folly::errnoStr(errno);
			write_offline = true;
			do_stop_on_error();
			return false;
		}
	}

	std::unique_lock lock(mutex);

	SCommitInfo commit_info;
	SFragInfo frag_info;
	frag_info.offset = transid;
	frag_info.len = disk_id;
	frag_info.action = FragAction::Commit;
	frag_info.commit_info = &commit_info;

	if (is_dead)
		return false;

	if (background_queue)
	{
		commit_background_queue.push_back(frag_info);
	}
	else
	{
		commit_queue.push_back(frag_info);
	}

	mdb_curr_sync = false;

	cond.notify_all();
	commit_info.commit_done.wait(lock);

	return commit_info.commit_errors == 0;
}

bool SingleFileStorage::empty_queue(bool background_queue)
{
	if (is_dead)
	{
		return false;
	}

	std::unique_lock lock(mutex);

	SCommitInfo commit_info;
	SFragInfo frag_info;
	frag_info.action = FragAction::EmptyQueue;
	frag_info.commit_info = &commit_info;

	if (background_queue)
	{
		commit_background_queue.push_back(frag_info);
	}
	else
	{
		commit_queue.push_back(frag_info);
	}

	cond.notify_all();
	commit_info.commit_done.wait(lock);

	return commit_info.commit_errors == 0;
}

bool SingleFileStorage::iter_start(int64_t disk_id, bool compressed, IterData& iter_data)
{
	CWData wdata;
	std::string prefix = std::to_string(disk_id) + "#";

	if (disk_id == 0)
	{
		if (compressed)
			wdata.addChar(0);
		else
			wdata.addChar(10);
	}
	else
	{
		if (compressed)
			wdata.addChar(1);
		else
			wdata.addBuffer(prefix.data(), prefix.size());
	}

	return iter_start(std::string(wdata.getDataPtr(), wdata.getDataSize()), false, iter_data);
}

bool SingleFileStorage::iter_start(bool compressed, IterData& iter_data)
{
	return iter_start(0, compressed, iter_data);
}

bool SingleFileStorage::iter_start(std::string fn, bool compressed, IterData& iter_data)
{
	if (is_dead)
	{
		return false;
	}

	if (compressed)
	{
		fn = compress_filename(fn);
	}

	mdb_madvise(db_env, 0);

	int rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &iter_data.iter_txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to open transaction handle for iteration (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}

	if(key_compare_func)
	{
		rc = mdb_set_compare(iter_data.iter_txn, dbi_main, key_compare_func);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Error setting main compare func in iter_start (" << mdb_strerror(rc) << ") sfs " << db_path;
			return false;
		}
	}

	rc = mdb_cursor_open(iter_data.iter_txn, dbi_main, &iter_data.iter_cur);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to open cursor for iteration (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}

	if(fn.empty())
	{
		rc = mdb_cursor_get(iter_data.iter_cur, &iter_data.iter_key, &iter_data.iter_val, MDB_FIRST);
	}
	else
	{
		iter_data.iter_key.mv_data = const_cast<char*>(fn.data());
		iter_data.iter_key.mv_size = fn.size();
		iter_data.iter_val.mv_data = nullptr;
		rc = mdb_cursor_get(iter_data.iter_cur, &iter_data.iter_key, &iter_data.iter_val, MDB_SET_RANGE);
	}

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get cursor for iteration (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}

	return true;
}

void SingleFileStorage::start_debug()
{
	/*iter_start("39640_73585ef100", true);

	int64_t offset;
	int64_t size;
	std::vector<SingleFileStorage::SPunchItem> extra_exts;
	int64_t last_modified;
	std::string fn;
	std::string md5sum;
	if (iter_curr_val(fn, offset, size, extra_exts, last_modified, md5sum))
	{
		XLOG(WARN) << "Debug val: " << decompress_filename(fn) << " offset " << std::to_string(offset) << " size " + std::to_string(size) << " last_modified " << std::to_string(last_modified) << " md5sum " << folly::hexlify(md5sum);
	}

	iter_stop();

	iter_start("39640_73585ef100", false);

	if (iter_curr_val(fn, offset, size, extra_exts, last_modified, md5sum))
	{
		XLOG(WARN) << "Debug val: " << decompress_filename(fn) << " offset " << std::to_string(offset) << " size " << std::to_string(size) << " last_modified " << std::to_string(last_modified) << " md5sum " << folly::hexlify(md5sum);
	}

	iter_stop();*/
}

void SingleFileStorage::iter_stop(IterData& iter_data)
{
	mdb_cursor_close(iter_data.iter_cur);
	mdb_txn_abort(iter_data.iter_txn);

	mdb_madvise(db_env, 1);
}

bool SingleFileStorage::iter_next(IterData& iter_data)
{
	int rc = mdb_cursor_get(iter_data.iter_cur, &iter_data.iter_key, &iter_data.iter_val, MDB_NEXT);

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get curosr next for iteration (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	else if (rc == MDB_NOTFOUND)
	{
		iter_data.iter_key.mv_data = nullptr;
	}

	return true;
}

bool SingleFileStorage::iter_curr_val(std::string & fn, int64_t& offset, int64_t & size,
	std::vector<SingleFileStorage::SPunchItem>& extra_exts, int64_t & last_modified, std::string & md5sum,
	const std::string& metadata, IterData& iter_data)
{
	if (iter_data.iter_key.mv_data == nullptr)
		return false;

	fn = std::string(reinterpret_cast<char*>(iter_data.iter_key.mv_data), iter_data.iter_key.mv_size);

	CRData rdata(reinterpret_cast<char*>(iter_data.iter_val.mv_data), iter_data.iter_val.mv_size);

	if (!rdata.getVarInt(&offset)
		|| !rdata.getVarInt(&size))
	{
		return false;
	}

	if(!read_extra_exts(offset, rdata, extra_exts))
		return false;

	if(!rdata.getVarInt(&last_modified)
		|| !rdata.getStr2(&md5sum))
	{
		return false;
	}

	return true;
}

bool SingleFileStorage::iter_curr_val(std::string & fn, std::string & data, IterData& iter_data)
{
	if (iter_data.iter_key.mv_data == nullptr)
		return false;

	fn = std::string(reinterpret_cast<char*>(iter_data.iter_key.mv_data), iter_data.iter_key.mv_size);

	data.assign(reinterpret_cast<char*>(iter_data.iter_val.mv_data), iter_data.iter_val.mv_size);

	return true;
}

int64_t SingleFileStorage::remove_fn(const std::string & fn, MDB_txn * txn, MDB_txn* freespace_txn,
	bool del_from_main, bool del_old, THREAD_ID tid)
{
	int64_t commit_errors = 0;

	MDB_cursor* mc;

	int rc = mdb_cursor_open(txn, del_old ? dbi_old : dbi_main, &mc);

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed mdb_cursor_open in remove_fn (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	SCOPE_EXIT{ mdb_cursor_close(mc); };

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed  mdb_cursor_open in remove_fn (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();

	MDB_val tval;

	rc = mdb_cursor_get(mc, &tkey, &tval, MDB_SET);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to get extent info in commit for del (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get extent info in commit for del (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else if (rc != MDB_NOTFOUND)
	{
		if (del_old)
		{
			XLOG(INFO) << "Del old fn " << decompress_filename(fn) << " sfs " << db_path;
		}

		CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

		int64_t fn_transid;
		if (del_old
			&& !rdata.getVarInt(&fn_transid))
		{
			XLOG(ERR) << "Cannot read transid in del_old";
			++commit_errors;
		}

		int64_t offset;
		int64_t length;
		if (rdata.getVarInt(&offset)
			&& rdata.getVarInt(&length))
		{
			std::vector<SPunchItem> extra_exts;
			if(!read_extra_exts(offset, rdata, extra_exts))
			{
				XLOG(ERR) << "LMDB: Error reading extra_exts " << db_path;
				++commit_errors;
				return commit_errors;
			}

			if(startup_finished)
			{
				std::lock_guard lock(mutex);
				auto it_reading = reading_items.find(offset);
				if(it_reading != reading_items.end())
				{
					assert(it_reading->second.refs>0);
					it_reading->second.free_skip = true;
					reading_free_skip_extents.insert(offset);
					for(const auto& ext: extra_exts)
					{
						reading_free_skip_extents.insert(ext.offset);
					}
				}
			}

			//Following can invalidate the memory of tval + rdata by spilling

			if (!add_freemap_ext(freespace_txn, offset, length, true, tid))
			{
				XLOG(ERR) << "LMDB: Failed to put free extent in commit sfs " << db_path;
				++commit_errors;
			}

			for (SPunchItem& eo: extra_exts)
			{
				if (!add_freemap_ext(freespace_txn, eo.offset, eo.len, true, tid))
				{
					XLOG(ERR) << "LMDB: Failed to put free extent in commit sfs " << db_path;
					++commit_errors;
				}
			}

			if (del_from_main)
			{
				rc = mdb_cursor_del(mc, 0);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to delete extent info in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB:Failed to delete extent info in commit (SIGBUS) sfs " << db_path;
					++commit_errors;
				}

				if(commit_errors==0 && with_rewrite)
					rewrite_npages(txn, mc, tid, n_rewrite_pages);
			}
		}
		else
		{
			XLOG(ERR) << "Cannot read offset+length in remove_fn()";
			++commit_errors;
		}
	}
	else if (del_from_main)
	{
		if(del_old)
		{
			XLOG(INFO) << "Did not find extent info for key \"" << decompress_filename(fn) 
				<< "\" sfs "<<db_path<<" del_old="<< std::to_string(del_old);
		}
		else
		{
			XLOG(DBG) << "Did not find extent info for key \"" << decompress_filename(fn) 
				<< "\" sfs "<<db_path<<" del_old="<< std::to_string(del_old);
		}
	}

	return commit_errors;
}

int64_t SingleFileStorage::restore_fn(const std::string & fn, MDB_txn * txn, MDB_txn* freespace_txn, THREAD_ID tid)
{
	int64_t commit_errors = 0;

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();

	MDB_val tval;

	int rc = mdb_get(txn, dbi_old, &tkey, &tval);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to get extent info in restore fn (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get extent info restore fn (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else if (rc != MDB_NOTFOUND)
	{
		CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size, true);

		commit_errors += remove_fn(fn, txn, freespace_txn, true, false, tid);

		int64_t fn_transid;
		rdata.getVarInt(&fn_transid);
		
		MDB_val val_offset;
		val_offset.mv_data = const_cast<char*>(rdata.getCurrDataPtr());
		val_offset.mv_size = rdata.getLeft();

		rc = mdb_put(txn, dbi_main, &tkey, &val_offset, 0);

		if (rc)
		{
			XLOG(ERR) << "Error restoring del item to main (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			return commit_errors;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "Error restoring del item to main (SIGBUS) sfs " << db_path;
			++commit_errors;
			return commit_errors;
		}

		rc = mdb_del(txn, dbi_old, &tkey, nullptr);

		if (rc)
		{
			XLOG(ERR) << "Error del restore item (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			return commit_errors;
		}
		else
		{
			XLOG(INFO) << "Restore fn " << decompress_filename(fn) << " sfs " << db_path;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "Error del restore item (SIGBUS) sfs " << db_path;
			++commit_errors;
			return commit_errors;
		}
	}
	else
	{
		XLOG(DBG) << "Did not find extent info for key \"" << decompress_filename(fn)
			<< "\" sfs " << db_path << " in restore";
	}

	return commit_errors;
}

int64_t SingleFileStorage::log_fn(const std::string & fn, MDB_txn * txn, THREAD_ID tid, int64_t transid)
{
	int64_t commit_errors = 0;

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();

	MDB_val tval;

	int rc = mdb_get(txn, dbi_main, &tkey, &tval);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to get extent info in log for del (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get extent info in log for del (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else if (rc != MDB_NOTFOUND)
	{
		CWData wdata;
		wdata.addVarInt(transid);
		wdata.addBuffer(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

		MDB_val tval_log;
		tval_log.mv_data = wdata.getDataPtr();
		tval_log.mv_size = wdata.getDataSize();

		rc = mdb_put(txn, dbi_old, &tkey, &tval_log, 0);

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to put log for del (SIGBUS) ("
				<< mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			return commit_errors;
		}

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to put in log for del (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		else
		{
			XLOG(INFO) << "Log fn " << decompress_filename(fn) << " sfs " << db_path;
		}
	}

	return commit_errors;
}

int64_t SingleFileStorage::add_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid, int64_t offset, int64_t len)
{
	int64_t commit_errors = 0;

	CWData keydata;
	keydata.addChar(0);
	keydata.addChar('t');
	keydata.addVarInt(idx);

	MDB_val tkey;
	tkey.mv_data = keydata.getDataPtr();
	tkey.mv_size = keydata.getDataSize();

	CWData valdata;
	valdata.addVarInt(0);
	valdata.addVarInt(offset);
	valdata.addVarInt(len);

	MDB_val tval;
	tval.mv_data = valdata.getDataPtr();
	tval.mv_size = valdata.getDataSize();

	int rc = mdb_put(txn, dbi_old, &tkey, &tval, 0);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put in add_tmp (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put in add_tmp (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else
	{
		XLOG(INFO) << "Add tmp idx " << idx << " off " << offset << " len " << len << " sfs " << db_path;
	}

	return commit_errors;
}

int64_t SingleFileStorage::rm_tmp(int64_t idx, MDB_txn* txn, THREAD_ID tid)
{
	CWData keydata;
	keydata.addChar(0);
	keydata.addChar('t');
	keydata.addVarInt(idx);

	MDB_val tkey;
	tkey.mv_data = keydata.getDataPtr();
	tkey.mv_size = keydata.getDataSize();

	int rc = mdb_del(txn, dbi_old, &tkey, nullptr);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to del in rm_tmp (SIGBUS) ("
			<< mdb_strerror(rc) << ") sfs " << db_path;
		return 1;
	}

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to del in rm_tmp (" << mdb_strerror(rc) << ") sfs " << db_path;
		return 1;
	}
	else
	{
		XLOG(INFO) << "Rm tmp idx " << idx << " sfs " << db_path;
	}

	return 0;
}

void SingleFileStorage::wait_queue(std::unique_lock<std::mutex>& lock, bool background_queue, bool defrag_check)
{
	size_t throttle_max = 1000;
	auto& sel_queue = background_queue ? commit_background_queue : commit_queue;
	while (sel_queue.size() > throttle_max
		&& !is_dead)
	{
		if (defrag_check
			&& defrag_restart == 1)
		{
			break;
		}

		lock.unlock();
		std::this_thread::sleep_for(10ms);
		lock.lock();
	}
}

#define FREEMAP_DEBUG(x) x

bool SingleFileStorage::add_freemap_ext(MDB_txn* txn, int64_t offset, int64_t len,
	bool used_in_curr_trans, THREAD_ID tid)
{
	if (is_dead)
		return false;

	if(offset == 0 && len == 0)
		return true;

	len = div_up(len, block_size)*block_size;

	MDB_cursor* fmap_cur;
	int rc = mdb_cursor_open(txn, dbi_free, &fmap_cur);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to open cursor for freemap ext (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to open cursor for freemap ext (SIGBUS) sfs " << db_path;
		is_dead = true;
		do_stop_on_error();
		return false;
	}

	CWData wdata_init;
	wdata_init.addVarInt(offset);

	MDB_val key;
	key.mv_data = wdata_init.getDataPtr();
	key.mv_size = wdata_init.getDataSize();

	MDB_val val;
	rc = mdb_cursor_get(fmap_cur, &key, &val, MDB_SET_RANGE);

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Failed to get curosr for freemap ext (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to get curosr for freemap ext (SIGBUS) sfs " << db_path;
		is_dead = true;
		do_stop_on_error();
		return false;
	}

	int64_t next_offset = -1;
	int64_t next_len = -1;
	bool merged_extent = false;
	if (rc != MDB_NOTFOUND)
	{
		CRData rdata(reinterpret_cast<char*>(key.mv_data), key.mv_size);
		CRData rdatalen(reinterpret_cast<char*>(val.mv_data), val.mv_size);
		if (rdata.getVarInt(&next_offset)
			&& rdatalen.getVarInt(&next_len))
		{
			CWData orig_data;
			orig_data.addVarInt(offset);

			if (offset + len > next_offset)
			{
				XLOG(WARN) << "Extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") already in freemap (next) as (" << std::to_string(next_offset) << ", " << std::to_string(next_len) << ") sfs " << db_path;
				//already in freemap
				//assert(false);
				mdb_cursor_close(fmap_cur);
				return true;
			}
			else if (offset + len == next_offset)
			{
				FREEMAP_DEBUG(XLOG(INFO) << "Merging new freemap extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") with next freemap ext at " << std::to_string(next_offset) << " new len " << folly::prettyPrint(next_len +len, folly::PRETTY_BYTES_IEC);)

				rc = mdb_cursor_del(fmap_cur, 0);

				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to del cursor current for freemap ext (" << mdb_strerror(rc) << ") sfs " << db_path;
					mdb_cursor_close(fmap_cur);
					return false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to del cursor current for freemap ext (SIGBUS) sfs " << db_path;
					return false;
				}

				merged_extent = true;
			}
			else
			{
				next_offset = -1;
			}
		}
		else
		{
			assert(false);
		}
	}

	rc = mdb_cursor_get(fmap_cur, &key, &val, MDB_PREV);

	if (rc != MDB_NOTFOUND)
	{
		CRData rdata(reinterpret_cast<char*>(key.mv_data), key.mv_size);
		CRData rdatalen(reinterpret_cast<char*>(val.mv_data), val.mv_size);
		int64_t prev_offset;
		int64_t prev_length;
		if (rdata.getVarInt(&prev_offset)
			&& rdatalen.getVarInt(&prev_length))
		{
			if ( prev_offset + prev_length > offset
				 && prev_offset <= offset )
			{
				XLOG(WARN) << "Extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") already in freemap (prev) as (" << std::to_string(prev_offset) << ", " << std::to_string(prev_length) << ") sfs " << db_path;
				//already in freemap
				//assert(false);
				mdb_cursor_close(fmap_cur);
				return true;
			}
			else if (offset == prev_offset + prev_length)
			{
				//Delete prev
				CWData del_val;
				del_val.addVarInt(prev_length);
				MDB_val m_del_val;
				m_del_val.mv_data = del_val.getDataPtr();
				m_del_val.mv_size = del_val.getDataSize();
				CWData del_key;
				del_key.addVarInt(prev_offset);
				MDB_val m_del_key;
				m_del_key.mv_data = del_key.getDataPtr();
				m_del_key.mv_size = del_key.getDataSize();
				rc = mdb_del(txn, dbi_free_len, &m_del_val, &m_del_key);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to del prev for freemap ext len (" << mdb_strerror(rc) << ") sfs " << db_path;
					mdb_cursor_close(fmap_cur);
					return false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to del prev for freemap ext len (SIGBUS) sfs " << db_path;
					return false;
				}

				CWData wdataval;
				if (!merged_extent)
				{
					FREEMAP_DEBUG(XLOG(INFO) << "Merging new freemap extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") with prev freemap ext at " << std::to_string(prev_offset)<<" new len "+folly::prettyPrint(prev_length +len, folly::PRETTY_BYTES_IEC); )
					wdataval.addVarInt(prev_length + len);
				}
				else
				{
					assert(next_len > 0);
					FREEMAP_DEBUG( XLOG(INFO) << "Merging new freemap extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") with prev and next freemap ext at " << std::to_string(prev_offset) << " new len " << folly::prettyPrint(prev_length + len+ next_len, folly::PRETTY_BYTES_IEC); )
					wdataval.addVarInt(prev_length + len + next_len);

					//Delete next
					CWData wdatanextlen;
					MDB_val nextlen;
					wdatanextlen.addVarInt(next_len);
					nextlen.mv_data = wdatanextlen.getDataPtr();
					nextlen.mv_size = wdatanextlen.getDataSize();

					CWData wdatanextoffset;
					wdatanextoffset.addVarInt(next_offset);
					MDB_val nextoffset;
					nextoffset.mv_data = wdatanextoffset.getDataPtr();
					nextoffset.mv_size = wdatanextoffset.getDataSize();

					rc = mdb_del(txn, dbi_free_len, &nextlen, &nextoffset);
					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to del next for freemap ext len (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
						mdb_cursor_close(fmap_cur);
						return false;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to del next for freemap ext len (1) (SIGBUS) sfs " << db_path;
						return false;
					}
				}

				val.mv_data = wdataval.getDataPtr();
				val.mv_size = wdataval.getDataSize();

				CWData wdatakey;
				wdatakey.addVarInt(prev_offset);
				key.mv_data = wdatakey.getDataPtr();
				key.mv_size = wdatakey.getDataSize();

				rc = mdb_cursor_put(fmap_cur, &key, &val, MDB_CURRENT);

				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to put cursor current for freemap ext (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
					mdb_cursor_close(fmap_cur);
					return false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to put cursor current for freemap ext (2) (SIGBUS) sfs " << db_path;
					return false;
				}

				rc = mdb_put(txn, dbi_free_len, &val, &key, 0);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to put prev for freemap ext len (" << mdb_strerror(rc) << ") sfs " << db_path;
					mdb_cursor_close(fmap_cur);
					return false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to put prev for freemap ext len (2) (SIGBUS) sfs " << db_path;
					return false;
				}

				merged_extent = true;
				next_offset = -1;
			}
		}
		else
		{
			assert(false);
		}
	}

	mdb_cursor_close(fmap_cur);

	if (!merged_extent 
		|| next_offset!=-1)
	{
		int64_t plen;
		if (next_offset == -1)
		{
			FREEMAP_DEBUG( XLOG(INFO) << "Unmerged new freemap extent (" << std::to_string(offset) << ", " << std::to_string(len) << ") new len " << folly::prettyPrint(len, folly::PRETTY_BYTES_IEC); )
			plen = len;
		}
		else
		{
			plen = len + next_len;

			//Delete next
			CWData wdatanextlen;
			MDB_val nextlen;
			wdatanextlen.addVarInt(next_len);
			nextlen.mv_data = wdatanextlen.getDataPtr();
			nextlen.mv_size = wdatanextlen.getDataSize();

			CWData wdatanextoffset;
			wdatanextoffset.addVarInt(next_offset);
			MDB_val nextoffset;
			nextoffset.mv_data = wdatanextoffset.getDataPtr();
			nextoffset.mv_size = wdatanextoffset.getDataSize();

			rc = mdb_del(txn, dbi_free_len, &nextlen, &nextoffset);
			if (rc)
			{
				XLOG(ERR) << "LMDB: Failed to del next for freemap ext len (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
				return false;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Failed to del next for freemap ext len (1) (SIGBUS) sfs " << db_path;
				return false;
			}
		}

		CWData wdata;
		wdata.addVarInt(offset);
		key.mv_data = wdata.getDataPtr();
		key.mv_size = wdata.getDataSize();

		CWData wdataval;
		wdataval.addVarInt(plen);

		val.mv_data = wdataval.getDataPtr();
		val.mv_size = wdataval.getDataSize();

		rc = mdb_put(txn, dbi_free, &key, &val, 0);

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to put freemap extent (" << mdb_strerror(rc) << ") sfs " << db_path;
			return false;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to put freemap extent (1) (SIGBUS) sfs " << db_path;
			return false;
		}

		rc = mdb_put(txn, dbi_free_len, &val, &key, 0);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to put freemap extent len (" << mdb_strerror(rc) << ") sfs " << db_path;
			return false;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to put freemap extent len (SIGBUS) sfs " << db_path;
			return false;
		}
	}

	{
		std::scoped_lock lock2(freespace_mutex);
		data_file_free += len;
	}

	if (used_in_curr_trans)
	{
		curr_new_free_extents.insert(offset);
	}

	return true;
}

bool SingleFileStorage::add_freemap_ext_simple(MDB_txn* txn, int64_t offset, int64_t len, THREAD_ID tid)
{
	if (is_dead)
		return false;

	len = div_up(len, block_size) * block_size;

	CWData wdata;
	wdata.addVarInt(offset);
	MDB_val key;
	key.mv_data = wdata.getDataPtr();
	key.mv_size = wdata.getDataSize();

	CWData wdataval;
	wdataval.addVarInt(len);

	MDB_val val;
	val.mv_data = wdataval.getDataPtr();
	val.mv_size = wdataval.getDataSize();

	int rc = mdb_put(txn, dbi_free, &key, &val, 0);

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put freemap extent in add_freemap_ext_simple(" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put freemap extent in add_freemap_ext_simple (1) (SIGBUS) sfs " << db_path;
		return false;
	}

	rc = mdb_put(txn, dbi_free_len, &val, &key, 0);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put freemap extent len in add_freemap_ext_simple (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put freemap extent len in add_freemap_ext_simple (SIGBUS) sfs " << db_path;
		return false;
	}

	return true;
}

bool SingleFileStorage::find_freemap_ext(MDB_txn* txn,
	THREAD_ID tid, int64_t & start, int64_t & len)
{
	len = 0;
	start = -1;

	int rc = mdb_set_compare(txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		XLOG(ERR) << "Error setting free len comparison function in find_freemap_ext (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}

	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_free_len, &it_cursor);
	MDB_val tkey;
	MDB_val tval;
	MDB_cursor_op op = MDB_FIRST;

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Error opening cursor in find_freemap_ext (SIGBUS) sfs " << db_path;
		return false;
	}

	std::vector<SPunchItem> local_free_skip_extents;
	{
		std::scoped_lock lock(mutex);
		local_free_skip_extents = curr_free_skip_extents;
	}

	while (rc == MDB_SUCCESS)
	{
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting cursor in find_freemap_ext (SIGBUS) sfs " << db_path;
			mdb_cursor_close(it_cursor);
			return false;
		}

		if (rc == MDB_SUCCESS)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			if (dval.getVarInt(&start)
				&& dkey.getVarInt(&len))
			{
				bool skip_ext = false;
				for (size_t i = 0; i < local_free_skip_extents.size(); ++i)
				{
					const SPunchItem& ext = local_free_skip_extents[i];
					if ((start > ext.offset
						&& start <= ext.offset+ext.len)
						|| (start+len > ext.offset
							&& start+len <= ext.offset + ext.len))
					{
						skip_ext = true;
						break;
					}
				}

				if (!skip_ext)
				{
					auto it = curr_new_free_extents.lower_bound(start);
					if (it == curr_new_free_extents.end()
						|| *it >= start + len)
					{
						//TODO perf: Seperate RW-Lock?
						std::scoped_lock lock(mutex);
						auto it = reading_free_skip_extents.lower_bound(start);
						if (it == reading_free_skip_extents.end()
							|| *it >= start + len)
						{
							break;
						}
					}
				}
			}
		}
	}

	mdb_cursor_close(it_cursor);

	if (rc == MDB_SUCCESS)
	{
		return true;
	}
	else if (rc == MDB_NOTFOUND)
	{
		return false;
	}
	
	XLOG(ERR) << "Error reading freemap ext len item (" << mdb_strerror(rc) << ") sfs " << db_path;
	assert(false);
	return false;
}

void SingleFileStorage::lock_defrag(const std::string & fn)
{
	std::scoped_lock lock(mutex);
	defrag_items.insert(fn);
}

bool SingleFileStorage::is_defrag_skip_item(const std::string & fn)
{
	std::scoped_lock lock(mutex);
	return defrag_skip_items.find(fn)!=defrag_skip_items.end();
}

void SingleFileStorage::unlock_defrag(const std::string & fn)
{
	std::scoped_lock lock(mutex);
	defrag_items.erase(fn);
}

void SingleFileStorage::wait_defrag(const std::string_view fn, std::unique_lock<std::mutex>& lock)
{
	while (defrag_items.find(std::string(fn)) != defrag_items.end())
	{
		lock.unlock();
		std::this_thread::sleep_for(10ms);
		lock.lock();
	}
}

void SingleFileStorage::setup_mmap_read_error(THREAD_ID tid)
{
	std::scoped_lock lock(mmap_read_error_mutex);
	mmap_read_error_jmp[tid] = std::pair<bool, std::vector<uintptr_t> >();
}

bool SingleFileStorage::clear_mmap_read_error(THREAD_ID tid)
{
	std::scoped_lock lock(mmap_read_error_mutex);
	auto it = mmap_read_error_jmp.find(tid);
	if (it == mmap_read_error_jmp.end())
	{
		return true;
	}
	else
	{
		bool ret = it->second.first;

		mmap_cleanup_addrs.insert(mmap_cleanup_addrs.end(),
			it->second.second.begin(),
			it->second.second.end());

		mmap_read_error_jmp.erase(it);
		if (ret)
		{
			XLOG(WARN) << "Had mmap read error (SIGBUS)";
		}
		return ret;
	}
}

bool SingleFileStorage::has_mmap_read_error_reset(THREAD_ID tid)
{
	std::scoped_lock lock(mmap_read_error_mutex);
	auto it = mmap_read_error_jmp.find(tid);
	if (it == mmap_read_error_jmp.end())
	{
		is_dead = true;
		do_stop_on_error();
		return true;
	}
	else
	{
		bool ret = it->second.first;

		mmap_cleanup_addrs.insert(mmap_cleanup_addrs.end(),
			it->second.second.begin(), it->second.second.end());

		it->second = std::pair<bool, std::vector<uintptr_t> >();
		if (ret)
		{
			is_dead = true;
			do_stop_on_error();
		}
		return ret;
	}
}

int64_t SingleFileStorage::reset_del_log_fn(MDB_txn * txn, MDB_txn* freespace_txn, THREAD_ID tid, int64_t disk_id, int64_t transid)
{
	int64_t commit_errors = 0;
	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_old, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	int rc;
	bool ret = true;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in reset del log fn(" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			break;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in reset del log fn (SIGBUS) sfs " << db_path;
			++commit_errors;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			std::string fn = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t curr_disk_id = get_fn_disk_id(fn);

			if (curr_disk_id != disk_id)
				continue;

			std::vector<char> data_buf(reinterpret_cast<char*>(tval.mv_data), 
				reinterpret_cast<char*>(tval.mv_data) + tval.mv_size);
			CRData rdata(data_buf.data(), data_buf.size());

			int64_t fn_transid = 0;
			rdata.getVarInt(&fn_transid);

			if (fn_transid == transid)
			{
				XLOG(INFO) << "Restoring key " << decompress_filename(fn) << " transid "<< std::to_string(fn_transid)<<" to previous data sfs " << db_path<<" curr transid "<<std::to_string(transid);

				commit_errors += remove_fn(fn, txn, freespace_txn, true, false, tid);

				MDB_val tval_offset;
				tval_offset.mv_size = data_buf.size() - rdata.getStreampos();
				tval_offset.mv_data = data_buf.data() + rdata.getStreampos();

				tkey.mv_data = &fn[0];
				tkey.mv_size = fn.size();

				rc = mdb_put(txn, dbi_main, &tkey, &tval_offset, 0);

				if (rc)
				{
					XLOG(ERR) << "Error adding del item to main (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
					continue;
				}

				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "Error del item to main (SIGBUS) sfs " << db_path;
					++commit_errors;
					continue;
				}
			}
			else
			{
				XLOG(INFO) << "Not restoring key " << decompress_filename(fn) << " transid "<<std::to_string(fn_transid)<<" to previous data sfs " << db_path<<" curr transid "<<std::to_string(transid);

				commit_errors += remove_fn(fn, txn, freespace_txn, false, true, tid);
			}

			rc = mdb_cursor_del(it_cursor, 0);

			if (rc)
			{
				XLOG(ERR) << "Error del del item (" << mdb_strerror(rc) << ") sfs " << db_path;
				++commit_errors;
				continue;
			}

			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "Error del del item (SIGBUS) sfs " << db_path;
				++commit_errors;
				continue;
			}

		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return commit_errors;
}

int64_t SingleFileStorage::reset_del_queue(MDB_txn * txn, MDB_txn* freespace_txn, THREAD_ID tid, int64_t disk_id, int64_t transid)
{
	int64_t commit_errors = 0;
	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_queue_del, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	int rc;
	bool ret = true;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in reset del queue (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			break;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in reset del queue (SIGBUS) sfs " << db_path;
			++commit_errors;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			std::string fn = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t curr_disk_id = get_fn_disk_id(fn);

			if (curr_disk_id != disk_id)
				continue;

			CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t fn_transid = 0;
			rdata.getVarInt(&fn_transid);

			if (fn_transid == transid)
			{
				XLOG(INFO) << "Not deleting queued " << decompress_filename(fn) << " transid " << std::to_string(fn_transid) << " sfs " << db_path << " curr transid " << std::to_string(transid);
			}
			else if(fn_transid>0)
			{
				XLOG(INFO) << "Deleting queued " << decompress_filename(fn) << " from main transid " << std::to_string(fn_transid) << " sfs " << db_path << " curr transid " << std::to_string(transid);

				commit_errors += remove_fn(fn, txn, freespace_txn, true, false, tid);
			}
			else
			{
				XLOG(INFO) << "Not deleting queued (transid<=0) " << decompress_filename(fn) << " from main transid " << std::to_string(fn_transid) << " sfs " << db_path << " curr transid " << std::to_string(transid);
			}

			rc = mdb_cursor_del(it_cursor, 0);

			if (rc)
			{
				XLOG(ERR) << "Error del del queue item (" << mdb_strerror(rc) << ") sfs " << db_path;
				++commit_errors;
				continue;
			}

			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "Error del del item (SIGBUS) sfs " << db_path;
				++commit_errors;
				continue;
			}

		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return commit_errors;
}

int64_t SingleFileStorage::reset_holes(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid)
{
	int64_t commit_errors = 0;
	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_holes, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	int rc;
	bool ret = true;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in reset holes (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
			break;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in reset holes (SIGBUS) sfs " << db_path;
			++commit_errors;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t start, len;

			if (rdata_key.getVarInt(&start)
				&& rdata_val.getVarInt(&len))
			{
				if (!add_freemap_ext(freespace_txn, start, len, false, tid))
					++commit_errors;

				rc = mdb_cursor_del(it_cursor, 0);

				if (rc)
				{
					XLOG(ERR) << "Error del in reset holes (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
					continue;
				}

				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "Error del in reset holes (SIGBUS) sfs " << db_path;
					++commit_errors;
					continue;
				}
			}
			
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return commit_errors;
}

void SingleFileStorage::wait_startup_finished(std::unique_lock<std::mutex> & lock)
{
	while (!startup_finished)
	{
		lock.unlock();
		std::this_thread::sleep_for(1s);
		lock.lock();
	}
}

void SingleFileStorage::free_extents(const std::vector<Ext>& extents)
{
	if (extents.empty())
		return;
	if(extents.size() == 1 && extents[0].data_file_offset==0 && extents[0].len == 0)
		return;

	std::unique_lock lock(mutex);
	wait_queue(lock, false, false);

	SFragInfo curr_frag(extents[0].data_file_offset, extents[0].len);
	for (size_t i = 1; i < extents.size(); ++i)
	{
		curr_frag.extra_exts.push_back(SPunchItem(extents[i].data_file_offset, extents[i].len));
	}
	curr_frag.action = FragAction::FreeExtents;

	commit_queue.push_back(curr_frag);

	cond.notify_all();
}

int64_t SingleFileStorage::get_really_min_space(int64_t& index_file_size)
{
	index_file_size = 0;
	{
		int fd = open((db_path + os_file_sep() + "index.lmdb").c_str(), O_RDONLY|O_CLOEXEC);
		if (fd!=-1)
		{
			index_file_size = fileSize(fd);
			close(fd);
		}
		else
		{
			XLOG(ERR) << "Error opening lmdb file " << db_path << os_file_sep() << "index.lmdb to get real size " << folly::errnoStr(errno);
			return false;
		}
	}

	if (cache_db_env != nullptr)
	{
		int fd = open( (freespace_cache_path + os_file_sep() + "freespace.lmdb").c_str(), O_RDONLY|O_CLOEXEC);
		if (fd!=-1)
		{
			index_file_size += fileSize(fd);
			close(fd);
		}
		else
		{
			XLOG(ERR) << "Error opening lmdb file " << freespace_cache_path << os_file_sep() << "freespace.lmdb to get real size " << folly::errnoStr(errno);
		}
	}

	int64_t really_min_space = index_file_size + 2LL * 1024 * 1024 * 1024;

	return really_min_space;
}

int64_t SingleFileStorage::get_burn_in_data_size()
{
	int64_t dsize = 0;
	int fd = open( (data_file_path.parent_path() / "burn_in.data").c_str(), O_RDONLY|O_CLOEXEC);
	if (fd!=-1)
	{
		dsize = fileSize(fd);
	}
	return dsize;
}

bool SingleFileStorage::do_free_minspace(MDB_txn* txn, MDB_txn* freespace_txn, THREAD_ID tid)
{
	int64_t free_space = os_free_space(data_file_path.string());
	free_space += get_burn_in_data_size();
	
	int64_t index_file_size;
	int64_t really_min_space = get_really_min_space(index_file_size);

	int64_t curr_min_space = min_free_space - index_file_size;

	int64_t skip_allowance = 512LL * 1024 * 1024;

	if (free_space >= curr_min_space - skip_allowance
		&& free_space >= really_min_space - skip_allowance)
	{
		return true;
	}

	int64_t freed_space = 0;
	std::string last_info;

	while (free_space>=0
		&& (free_space < curr_min_space
			|| free_space<really_min_space)
		&& punch_holes)
	{
		last_info = "Free space (" + folly::prettyPrint(free_space, folly::PRETTY_BYTES_IEC) + ") not enough (" + folly::prettyPrint(curr_min_space, folly::PRETTY_BYTES_IEC) + " / " + folly::prettyPrint(min_free_space, folly::PRETTY_BYTES_IEC) + " / " + folly::prettyPrint(really_min_space, folly::PRETTY_BYTES_IEC) + ").";
		XLOG(INFO) << last_info << " Punching...";

		int64_t start, len;
		if (find_freemap_ext(freespace_txn, tid, start, len))
		{
			XLOG(INFO) << "Punching extent (" << std::to_string(start) << ", " << std::to_string(len) << ") size "+folly::prettyPrint(len, folly::PRETTY_BYTES_IEC);

			if (!data_file.punchHole(start, len))
			{
				XLOG(ERR) << "Error punching hole (" << std::to_string(start) << ", " << std::to_string(len) << ") file " << data_file_path << ". " << folly::errnoStr(errno);
				return false;
			}

			MDB_val tkey;
			CWData wtkey;
			wtkey.addVarInt(start);
			tkey.mv_data = wtkey.getDataPtr();
			tkey.mv_size = wtkey.getDataSize();

			MDB_val tval;
			CWData wtval;
			wtval.addVarInt(len);
			tval.mv_data = wtval.getDataPtr();
			tval.mv_size = wtval.getDataSize();

			int rc = mdb_put(txn, dbi_holes, &tkey, &tval, 0);

			bool mmap_err = has_mmap_read_error_reset(tid);
			if (mmap_err)
			{
				XLOG(ERR) << "LMDB: Error adding hole extent (SIGBUS) sfs " << db_path;
				return false;
			}

			if (rc)
			{
				XLOG(ERR) << "Error adding hole extent (" << mdb_strerror(rc) << ") sfs " << db_path;
				return false;
			}
			
			rc = mdb_del(freespace_txn, dbi_free, &tkey, nullptr);

			mmap_err = has_mmap_read_error_reset(tid);
			if (mmap_err)
			{
				XLOG(ERR) << "LMDB: Error removing free extent -2 (SIGBUS) sfs " << db_path;
				return false;
			}

			if (rc)
			{
				XLOG(ERR) << "Error removing free extent -2 (" << mdb_strerror(rc) << ") sfs " << db_path;
				return false;
			}
			else if (!mmap_err)
			{
				rc = mdb_del(freespace_txn, dbi_free_len, &tval, &tkey);

				if (rc)
				{
					XLOG(ERR) << "Error removing free extent (4) (" << (std::string)mdb_strerror(rc) << ") sfs " << db_path;
					return false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error removing free extent (4) (SIGBUS) sfs " << db_path;
					return false;
				}
			}

			{
				std::scoped_lock lock2(freespace_mutex);
				data_file_free -= len;
			}

			freed_space += len;
			free_space = os_free_space(data_file_path.string());
			free_space += get_burn_in_data_size();
		}
		else
		{
			break;
		}
	}

	if (freed_space > 0)
	{
		XLOG(WARN) << "Freed " << folly::prettyPrint(freed_space, folly::PRETTY_BYTES_IEC) << " via punching holes. " << last_info;
	}

	return true;
}

int64_t SingleFileStorage::queue_del(const std::string & fn, MDB_txn * txn, THREAD_ID tid, int64_t transid)
{
	int64_t commit_errors = 0;

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();
	
	CWData wdata;
	wdata.addVarInt(transid);

	MDB_val tval_log;
	tval_log.mv_data = wdata.getDataPtr();
	tval_log.mv_size = wdata.getDataSize();

	int rc = mdb_put(txn, dbi_queue_del, &tkey, &tval_log, 0);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put queue for del (SIGBUS) ("
			 << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put in queue for del (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else
	{
		XLOG(INFO) << "Queue del fn " <<  decompress_filename(fn) << " sfs " << db_path;
	}

	return commit_errors;
}

int64_t SingleFileStorage::unqueue_del(const std::string & fn, MDB_txn * txn, THREAD_ID tid)
{
	int64_t commit_errors = 0;

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();

	int rc = mdb_del(txn, dbi_queue_del, &tkey, NULL);

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to del queue for del (SIGBUS) ("
			 << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
		return commit_errors;
	}

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to del in unqueue for del (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	else
	{
		XLOG(INFO) << "Unqueue del fn " << decompress_filename(fn) << " sfs " << db_path;
	}

	return commit_errors;
}

void SingleFileStorage::add_defrag_skip_items_queue()
{
	for (SFragInfo& fi : commit_queue)
	{
		if (!fi.fn.empty())
		{
			defrag_skip_items.insert(fi.fn);
		}
	}

	for (SFragInfo& fi : commit_background_queue)
	{
		if (!fi.fn.empty())
		{
			defrag_skip_items.insert(fi.fn);
		}
	}
}

bool SingleFileStorage::open_cache_db(int64_t current_txn_id, int64_t mapsize, bool use_other, bool del_create, MDB_txn*& freespace_txn)
{
	if (cache_db_env != nullptr)
	{
		if (freespace_txn != nullptr)
		{
			mdb_txn_abort(freespace_txn);
			freespace_txn = nullptr;
		}
		mdb_env_close(cache_db_env);
		cache_db_env = nullptr;
	}

	THREAD_ID tid = gettid();

	int rc = mdb_env_create(&cache_db_env);

	std::string cache_db_name = "freespace.lmdb";

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to create cache LMDB env (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_maxreaders(cache_db_env, 4094);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set cache max readers (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_mapsize(cache_db_env, mapsize * 2);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set cache map size (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_env_set_maxdbs(cache_db_env, 3);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to set cache max dbs (" + (std::string)mdb_strerror(rc) + ")");
	}

	unsigned int mdb_flags = MDB_NOSUBDIR | MDB_NOTLS | MDB_NORDAHEAD;

	if (del_create)
	{
		std::error_code ec;
		std::filesystem::remove(freespace_cache_path + os_file_sep() + cache_db_name, ec);
		std::filesystem::remove(freespace_cache_path + os_file_sep() + cache_db_name + "-lock", ec);
	}

	if (use_other)
	{
		mdb_flags |= MDB_PREVSNAPSHOT;
	}

	if (std::filesystem::exists("/var/urbackup/sfs_use_lmdb_writemap"))
	{
		mdb_flags |= MDB_WRITEMAP;
	}

	rc = mdb_env_open(cache_db_env, (freespace_cache_path + os_file_sep() + cache_db_name).c_str(), mdb_flags, 0664);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to open cache LMDB database file (" + (std::string)mdb_strerror(rc) + ")");
	}

	size_t txnid1, txnid2;
	mdb_env_get_txnids(cache_db_env, &txnid1, &txnid2);
	if(use_other)
	{
		XLOG(INFO) << "Opened other cache db txn, txnid1="<<std::to_string(txnid1)<<" txnid2="+std::to_string(txnid2);
	}
	else
	{
		XLOG(INFO) << "Opened current cache db txn, txnid1=" << std::to_string(txnid1) << " txnid2=" + std::to_string(txnid2);
	}

	rc = mdb_txn_begin(cache_db_env, NULL, del_create ? 0 : MDB_RDONLY, &freespace_txn);

	if (rc)
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for cache dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Failed to open transaction handle for cache dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(freespace_txn, "free", del_create ? MDB_CREATE : 0, &dbi_free);
	if(rc==MDB_NOTFOUND)
	{
		XLOG(INFO) << "Could not find free cache db (MDB_NOTFOUND). Regenerating free space cache...";
		mdb_txn_abort(freespace_txn);
		freespace_txn=nullptr;
		regen_freespace_cache = true;
		mdb_env_close(cache_db_env);
                cache_db_env = nullptr;
                return false;
	}

	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening cache free dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening cache free dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(freespace_txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting cache free comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_dbi_open(freespace_txn, "size", del_create ? MDB_CREATE : 0, &dbi_cache_size);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening cache size dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening cache size dbi handle (SIGBUS)");
	}

	rc = mdb_dbi_open(freespace_txn, "free_len", (del_create ? MDB_CREATE : 0 ) | MDB_DUPSORT, &dbi_free_len);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening cache free_len dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening cache free_len dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(freespace_txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting cache free_len comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	MDB_val val;
	char ch = dbi_size_info_size;
	val.mv_data = &ch;
	val.mv_size = 1;

	MDB_val size_out;

	rc = mdb_get(freespace_txn, dbi_cache_size, &val, &size_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		throw std::runtime_error("LMDB: Error getting cache data file max size (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error getting cache data file max size (SIGBUS)");
	}

	if (rc != MDB_NOTFOUND)
	{
		mdb_txn_abort(freespace_txn);
		freespace_txn=nullptr;

		int64_t lmdb_transid = -1;
		if (size_out.mv_size == sizeof(lmdb_transid))
		{
			memcpy(&lmdb_transid, size_out.mv_data, sizeof(data_file_offset));
			XLOG(INFO) << "LMDB transid: " << std::to_string(lmdb_transid);
		}
		else
		{
			throw std::runtime_error("Cache size data has wrong size");
		}

		if (lmdb_transid == current_txn_id)
		{
			XLOG(INFO) << "Ext freespace cache db transid ok ("<<std::to_string(lmdb_transid)<<" == "<<std::to_string(current_txn_id)<<")";
			return true;
		}
		else
		{
			XLOG(INFO) << "Ext freespace cache db transid not ok (" << std::to_string(lmdb_transid) << " == "<<std::to_string(current_txn_id)<<", "<< freespace_cache_path <<")";
			mdb_env_close(cache_db_env);
			freespace_txn = nullptr;
			cache_db_env = nullptr;
			return false;
		}
	}
	else
	{
		if(!del_create)
		{
			mdb_txn_abort(freespace_txn);
			freespace_txn=nullptr;
		}

		XLOG(INFO) << "Cache size info not found (" << freespace_cache_path << "). Regenerating free space cache.";
		regen_freespace_cache = true;
		return true;
	}
}

bool SingleFileStorage::generate_freespace_cache(MDB_txn* source_txn, MDB_txn* dst_txn, bool fast_gen)
{
	bool ignore_errors = std::filesystem::exists("/etc/urbackup/generate_freespace_cache_ignore_errors");

	THREAD_ID tid = gettid();

	MDB_val tkey;
	int rc;

	int64_t data_size = data_file.size();
	int64_t bitmap_size = div_up(data_size, 4096);

	TmpMmapedFileBitmap bmap(bitmap_size, false);

	auto add_extent_bmap = [ignore_errors, &bmap](MDB_val& tkey, MDB_val& tval) {
		CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

		SPunchItem first_ext;

		if (!rdata.getVarInt(&first_ext.offset)
			|| !rdata.getVarInt(&first_ext.len))
		{
			return true;
		}

		std::vector<SPunchItem> extents;

		if(!read_extra_exts(first_ext.offset, rdata, extents))
		{
			std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			XLOG(ERR) << "Error getting extents of " << decompress_filename(key_name) << " in generate_freespace_cache";
			return true;
		}

		bool ret = true;
		for (size_t i = 0; i < extents.size() + 1; ++i)
		{
			SPunchItem& ext = (i == 0) ? first_ext : extents[i - 1];

			int64_t start_bit = ext.offset / 4096;
			int64_t len_bits = div_up(ext.len, 4096);

			if (bmap.get_range(start_bit, start_bit + len_bits))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Range of " << decompress_filename(key_name) << " already set offset=" << std::to_string(ext.offset) << " len=" + std::to_string(ext.len);
				if (!ignore_errors)
					ret = false;
			}

			bmap.set_range(start_bit, start_bit + len_bits, true);
		}

		return ret;
	};

	bool ret = true;

	MDB_cursor* it_cursor;
	mdb_cursor_open(source_txn, dbi_main, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;

	if (fast_gen)
	{
		XLOG(INFO) << "Reading page ids from " << db_path;

		TmpMmapedPgIds page_ids;

		if (!read_pgids(source_txn, dbi_main, tid, page_ids))
		{
			XLOG(ERR) << "Reading page ids from " << db_path << " failed";
			mdb_cursor_close(it_cursor);
			return false;
		}

		XLOG(INFO) << "Iterating through "<< std::to_string(page_ids.size()) << " pages " << db_path;

		for (size_t* pg_it = page_ids.begin(); pg_it != page_ids.end(); ++pg_it)
		{
			unsigned int nkeys;
			rc = mdb_page_get_nkeys(it_cursor, *pg_it, &nkeys);

			if (rc)
			{
				XLOG(ERR) << "LMDB: Error getting mdb_page_get_nkeys in generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
				ret = false;
				break;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Error getting item in mdb_page_get_nkeys in generate_freespace_cache (SIGBUS) sfs " << db_path;
				ret = false;
				break;
			}

			for (unsigned int idx = 0; idx < nkeys; ++idx)
			{
				MDB_val tval;
				rc = mdb_page_get_val(it_cursor, *pg_it, idx, &tkey, &tval);

				if (rc)
				{
					XLOG(ERR) << "LMDB: Error getting item mdb_page_get_val in main generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error getting item mdb_page_get_val in main generate_freespace_cache (SIGBUS) sfs " << db_path;
					ret = false;
					break;
				}

				if (!add_extent_bmap(tkey, tval))
					ret = false;
			}

			if (!ret)
				break;
		}
	}
	else
	{
		do
		{
			MDB_val tval;
			rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
			op = MDB_NEXT;

			if (rc && rc != MDB_NOTFOUND)
			{
				XLOG(ERR) << "LMDB: Error getting item in main generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
				ret = false;
				break;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Error getting item in main generate_freespace_cache (SIGBUS) sfs " << db_path;
				ret = false;
				break;
			}

			if (rc != MDB_NOTFOUND)
			{
				if (!add_extent_bmap(tkey, tval))
					ret = false;
			}
		} while (rc != MDB_NOTFOUND);
	}

	mdb_cursor_close(it_cursor);

	if (!ret)
		return ret;

	mdb_cursor_open(source_txn, dbi_old, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in old generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in old generate_freespace_cache (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			SPunchItem first_ext;

			int64_t transid;
			if (!rdata.getVarInt(&transid)
				|| !rdata.getVarInt(&first_ext.offset)
				|| !rdata.getVarInt(&first_ext.len))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Error getting first old extent of " << decompress_filename(key_name) << " in generate_freespace_cache";
				continue;
			}
			
			std::vector<SPunchItem> extents;
			if(!read_extra_exts(first_ext.offset, rdata, extents))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Error getting old extents of " << decompress_filename(key_name) << " in generate_freespace_cache";
				continue;
			}

			for (size_t i = 0; i < extents.size() + 1; ++i)
			{
				SPunchItem& ext = (i == 0) ? first_ext : extents[i - 1];

				int64_t start_bit = ext.offset / 4096;
				int64_t len_bits = div_up(ext.len, 4096);

				if (bmap.get_range(start_bit, start_bit + len_bits))
				{
					std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
					XLOG(ERR) << "Range of " << decompress_filename(key_name) << " already set offset=" << std::to_string(ext.offset) << " len=" + std::to_string(ext.len);
					if (!ignore_errors)
						ret = false;
				}

				bmap.set_range(start_bit, start_bit + len_bits, true);
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	if (!ret)
		return ret;

	mdb_cursor_open(source_txn, dbi_holes, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in holes generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in holes generate_freespace_cache (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t offset, len;
			if (rdata_key.getVarInt(&offset)
				&& rdata_val.getVarInt(&len))
			{
				int64_t start_bit = offset / 4096;
				int64_t len_bits = div_up(len, 4096);

				if(bmap.get_range(start_bit, start_bit + len_bits))
				{
					XLOG(ERR) << "Range of hole already set offset="<<std::to_string(offset) <<" len="<<std::to_string(len);
					if (!ignore_errors)
						ret=false;
				}

				bmap.set_range(start_bit, start_bit + len_bits, true);
			}
			else
			{
				XLOG(ERR) << "Error parsing hole data";
			}
		}

	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	if (!ret)
		return ret;

	if(data_file_offset_end>0)
	{
		int64_t curr_end = (std::min)(data_size, data_file_offset_end);
		int64_t start_bit = data_file_offset/4096;
		int64_t len_bits = div_up(curr_end - data_file_offset, 4096);

		if(bmap.get_range(start_bit, start_bit + len_bits))
		{
			XLOG(ERR) << "Range of data_file_offset+end already set offset=" << std::to_string(data_file_offset) << " len=" << std::to_string(curr_end - data_file_offset);
			if (!ignore_errors)
				ret=false;
		}

		bmap.set_range(start_bit, start_bit + len_bits, true);
	}

	int64_t free_start = -1;
	for (int64_t i = 0; i < bitmap_size; ++i)
	{
		bool bit_set = bmap.get(i);

		if (!bit_set
			&& free_start == -1)
		{
			free_start = i;
		}
		else if (free_start != -1
			&& bit_set)
		{
			int64_t free_len = i - free_start;
			
			if (!add_freemap_ext_simple(dst_txn, free_start * 4096, free_len * 4096 - 1, tid))
			{
				ret = false;
				XLOG(ERR) << "Error adding freemap ext " << std::to_string(free_start) << " len " << std::to_string(free_len);
			}
			free_start=-1;
		}
	}

	if (free_start != -1)
	{
		int64_t free_len = bitmap_size - free_start;

		if (!add_freemap_ext_simple(dst_txn, free_start * 4096, free_len * 4096 - 1, tid))
		{
			ret = false;
			XLOG(ERR) << "Error adding last freemap ext " << std::to_string(free_start) << " len " << std::to_string(free_len);
		}
	}

	if(source_txn == dst_txn)
	{
		return ret;
	}

	char ch = 0;
	tkey.mv_data = &ch;
	tkey.mv_size = 1;

	MDB_val tval;
	int64_t tdata;

	tdata = mdb_get_txnid(source_txn);

	tval.mv_data = &tdata;
	tval.mv_size = sizeof(tdata);

	rc = mdb_put(dst_txn, dbi_cache_size, &tkey, &tval, 0);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put cache size in generate_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put cache size in generate_freespace_cache (SIGBUS) sfs " << db_path;
		ret = false;
	}

	return ret;
}

bool SingleFileStorage::freespace_check(MDB_txn* source_txn, MDB_txn* freespace_txn, bool fast_check)
{
	THREAD_ID tid = gettid();

	MDB_val tkey;
	int rc;

	int64_t data_size = data_file.size();
	int64_t bitmap_size = div_up(data_size, 4096);

	TmpMmapedFileBitmap bmap(bitmap_size, false);

	auto set_extent_bmap_val = [&bmap](MDB_val& tkey, MDB_val& tval) {
		CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

		SPunchItem first_ext;

		if (!rdata.getVarInt(&first_ext.offset)
			|| !rdata.getVarInt(&first_ext.len))
		{
			std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			XLOG(ERR) << "Error reading first ext " << decompress_filename(key_name);
			return false;
		}

		std::vector<SPunchItem> extents;

		if(!read_extra_exts(first_ext.offset, rdata, extents))
		{
			std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			XLOG(ERR) << "Error getting extents of " << decompress_filename(key_name) << " in freespace_check";
			return false;
		}

		bool ret = true;
		for (size_t i = 0; i < extents.size() + 1; ++i)
		{
			SPunchItem& ext = (i == 0) ? first_ext : extents[i - 1];

			int64_t start_bit = ext.offset / 4096;
			int64_t len_bits = div_up(ext.len, 4096);

			if (bmap.get_range(start_bit, start_bit + len_bits))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Range of " << decompress_filename(key_name) << " already set offset=" << std::to_string(ext.offset) << " len=" << std::to_string(ext.len);
				ret = false;
			}

			bmap.set_range(start_bit, start_bit + len_bits, true);
		}
		return ret;
	};

	bool ret = true;

	XLOG(INFO) << "Freespace check sfs " << db_path << " setting bitmap using main...";

	MDB_cursor* it_cursor;
	mdb_cursor_open(source_txn, dbi_main, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;

	if (fast_check)
	{
		XLOG(INFO) << "Reading page ids from " << db_path;

		TmpMmapedPgIds page_ids;

		if (!read_pgids(source_txn, dbi_main, tid, page_ids))
		{
			XLOG(ERR) << "Reading page ids from " << db_path << " failed";
			mdb_cursor_close(it_cursor);
			return false;
		}

		XLOG(INFO) << "Iterating through " << std::to_string(page_ids.size()) << " pages " << db_path;

		for (size_t* pg_it = page_ids.begin(); pg_it != page_ids.end(); ++pg_it)
		{
			unsigned int nkeys;
			rc = mdb_page_get_nkeys(it_cursor, *pg_it, &nkeys);

			if (rc)
			{
				XLOG(ERR) << "LMDB: Error getting mdb_page_get_nkeys (" << mdb_strerror(rc) << ") sfs " << db_path;
				ret = false;
				break;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Error getting item in mdb_page_get_nkeys (SIGBUS) sfs " << db_path;
				ret = false;
				break;
			}

			for (unsigned int idx = 0; idx < nkeys; ++idx)
			{
				MDB_val tval;
				rc = mdb_page_get_val(it_cursor, *pg_it, idx, &tkey, &tval);

				if (rc)
				{
					XLOG(ERR) << "LMDB: Error getting item mdb_page_get_val in main freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error getting item mdb_page_get_val in main freespace_check (SIGBUS) sfs " << db_path;
					ret = false;
					break;
				}

				if (!set_extent_bmap_val(tkey, tval))
					ret = false;
			}

			if (!ret)
				break;
		}
	}
	else
	{
		do
		{
			MDB_val tval;
			rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
			op = MDB_NEXT;

			if (rc && rc != MDB_NOTFOUND)
			{
				XLOG(ERR) << "LMDB: Error getting item in main freespace_check (" + (std::string)mdb_strerror(rc) + ") sfs " << db_path;
				ret = false;
				break;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Error getting item in main freespace_check (SIGBUS) sfs " << db_path;
				ret = false;
				break;
			}

			if (rc != MDB_NOTFOUND)
			{
				if (!set_extent_bmap_val(tkey, tval))
					ret = false;
			}
		} while (rc != MDB_NOTFOUND);
	}

	mdb_cursor_close(it_cursor);

	if (!ret)
		return ret;

	mdb_cursor_open(source_txn, dbi_old, &it_cursor);
	op = MDB_FIRST;

	XLOG(INFO) << "Freespace check sfs " << db_path << " setting bitmap using old...";
	int64_t old_items = 0;
	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in old freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in old freespace_check (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			++old_items;
			CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			SPunchItem first_ext;

			int64_t transid;

			if (!rdata.getVarInt(&transid)
				|| !rdata.getVarInt(&first_ext.offset)
				|| !rdata.getVarInt(&first_ext.len))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Error reading first old ext " << decompress_filename(key_name);
				ret = false;
				continue;
			}

			std::vector<SPunchItem> extents;

			if(!read_extra_exts(first_ext.offset, rdata, extents))
			{
				std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Error getting old extents of " << decompress_filename(key_name) << " in freespace_check";
				ret = false;
				continue;
			}

			for (size_t i = 0; i < extents.size() + 1; ++i)
			{
				SPunchItem& ext = (i == 0) ? first_ext : extents[i - 1];

				int64_t start_bit = ext.offset / 4096;
				int64_t len_bits = div_up(ext.len, 4096);

				if (bmap.get_range(start_bit, start_bit + len_bits))
				{
					std::string key_name = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
					XLOG(ERR) << "Range of " << decompress_filename(key_name) << " already set (old) offset=" << std::to_string(ext.offset) + " len=" << std::to_string(ext.len);
					ret = false;
				}

				bmap.set_range(start_bit, start_bit + len_bits, true);
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	XLOG(INFO) << "Freespace check sfs " << db_path << " setting bitmap using holes... (old_items=" << std::to_string(old_items) << ")";
	int64_t n_holes = 0;
	mdb_cursor_open(source_txn, dbi_holes, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in holes freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in holes freespace_check (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			++n_holes;
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t offset, len;
			if (rdata_key.getVarInt(&offset)
				&& rdata_val.getVarInt(&len))
			{
				int64_t start_bit = offset / 4096;
				int64_t len_bits = div_up(len, 4096);

				if (start_bit + len_bits >= bitmap_size)
				{
					XLOG(ERR) << "Freespace check main offset too large offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path;
					ret = false;
					continue;
				}

				if(bmap.get_range(start_bit, start_bit + len_bits))
				{
					XLOG(ERR) << "Range of hole already set offset=" << std::to_string(offset) << " len=" << std::to_string(len);
					ret=false;
				}

				bmap.set_range(start_bit, start_bit + len_bits, true);
			}
			else
			{
				XLOG(ERR) << "Error parsing hole data in freespace_check";
				ret = false;
			}
		}

	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	XLOG(INFO) << "Freespace check sfs " << db_path << " setting bitmap using holes (alt iteration order, n_holes="<<std::to_string(n_holes)<< ")...";

	rc = mdb_set_compare(source_txn, dbi_holes, mdb_cmp_memn);
	if (rc)
	{
		XLOG(ERR) << "Error setting holes comparison function (" << (std::string)mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	mdb_cursor_open(source_txn, dbi_holes, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in holes freespace_check (" << (std::string)mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in holes freespace_check (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t offset, len;
			if (rdata_key.getVarInt(&offset)
				&& rdata_val.getVarInt(&len))
			{
				int64_t start_bit = offset / 4096;
				int64_t len_bits = div_up(len, 4096);

				if (start_bit + len_bits >= bitmap_size)
				{
					XLOG(ERR) << "Freespace check main offset too large offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path;
					ret = false;
					continue;
				}

				if (!bmap.get_range(start_bit, start_bit + len_bits))
				{
					XLOG(ERR) << "Range of hole not already set offset=" << std::to_string(offset) << " len=" << std::to_string(len);
				}

				bmap.set_range(start_bit, start_bit + len_bits, true);
			}
			else
			{
				XLOG(ERR) << "Error parsing hole data in freespace_check";
				ret = false;
			}
		}

	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	rc = mdb_set_compare(source_txn, dbi_holes, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting holes comparison function (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	if(data_file_offset_end>0)
	{
		int64_t curr_end = (std::min)(data_size, data_file_offset_end);
		int64_t start_bit = data_file_offset/4096;
		int64_t len_bits = div_up(curr_end - data_file_offset, 4096);

		if(bmap.get_range(start_bit, start_bit + len_bits))
		{
			XLOG(ERR) << "Range of data_file_offset+end already set offset=" << std::to_string(data_file_offset) << " len=" << std::to_string(curr_end - data_file_offset);
			ret=false;
		}

		bmap.set_range(start_bit, start_bit + len_bits, true);
	}


	mdb_cursor_open(freespace_txn, dbi_free, &it_cursor);
	op = MDB_FIRST;

	XLOG(INFO) << "Freespace check sfs " << db_path << " setting bitmap using freespace...";
	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in free freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in free freespace_check (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t offset, len;
			if (rdata_key.getVarInt(&offset)
				&& rdata_val.getVarInt(&len))
			{
				if (div_up(offset + len, 4096) > bitmap_size)
				{
					XLOG(ERR) << "Freespace check offset too large offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path+" "<<std::to_string(div_up(offset + len, 4096)) << ">" << std::to_string(bitmap_size);
					ret = false;
					continue;
				}

				bool b = bmap.get_range(offset / 4096, div_up(offset + len, 4096));
				if (b)
				{
					XLOG(ERR) << "Freespace check freespace is set offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path;
					ret = false;
				}
				bmap.set_range(offset / 4096, div_up(offset + len, 4096), true);

				MDB_cursor* it_cursor2;
				mdb_cursor_open(freespace_txn, dbi_free_len, &it_cursor2);

				rc = mdb_cursor_get(it_cursor2, &tval, &tkey, MDB_GET_BOTH);

				if (rc && rc != MDB_NOTFOUND)
				{
					XLOG(ERR) << "LMDB: Error getting item in free len freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error getting item in free len freespace_check (SIGBUS) sfs " << db_path;
					ret = false;
					break;
				}

				if (rc == MDB_NOTFOUND)
				{
					XLOG(ERR) << "Freespace check freespace item not found in len idx offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path;
					ret = false;
				}

				mdb_cursor_close(it_cursor2);
			}
			else
			{
				XLOG(ERR) << "Error parsing free data in freespace_check";
				ret = false;
			}
		}

	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	XLOG(INFO) << "Freespace check sfs " << db_path << " checking freespace len idx...";

	mdb_cursor_open(freespace_txn, dbi_free_len, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freelen freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in freelen freespace_check (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData rdata_val(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
			CRData rdata_key(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

			int64_t offset, len;
			if (rdata_key.getVarInt(&len)
				&& rdata_val.getVarInt(&offset))
			{
				MDB_val tval_out;
				rc = mdb_get(freespace_txn, dbi_free, &tval, &tval_out);

				if (rc && rc != MDB_NOTFOUND)
				{
					XLOG(ERR) << "LMDB: Error getting item in free len free freespace_check (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error getting item in free len free freespace_check (SIGBUS) sfs " << db_path;
					ret = false;
					break;
				}

				CRData rdata_tval_out(reinterpret_cast<char*>(tval_out.mv_data), tval_out.mv_size);

				int64_t tval_len;
				if (rc == MDB_NOTFOUND
					|| !rdata_tval_out.getVarInt(&tval_len)
					|| tval_len!=len)
				{
					XLOG(ERR) << "Freespace check free len item not found in free offset " << std::to_string(offset) << " len " << std::to_string(len) << " sfs " << db_path;
					ret = false;
				}
			}
			else
			{
				XLOG(ERR) << "Error parsing free data in freespace_check";
				ret = false;
			}
		}

	} while (rc != MDB_NOTFOUND);

	XLOG(INFO) << "Freespace check sfs " << db_path << " checking bitmap...";

	int64_t notset_size = 0;
	int64_t notset_start=-1;
	for (size_t i = 0; i < bitmap_size; ++i)
	{
		bool bset=bmap.get(i);
		if (!bset
			&& notset_start==-1)
		{
			notset_start = i;
		}
		else if(bset
			&& notset_start!=-1)
		{
			int64_t len = i - notset_start;
			XLOG(ERR) << "Freespace check sfs " << db_path << " bits not set start " << std::to_string(notset_start * 4096)<<
				" len " << std::to_string(len*4096) << " ("+folly::prettyPrint(len*4096, folly::PRETTY_BYTES_IEC)+") "<<
				" ("+std::to_string(i)<<"/"<<std::to_string(bitmap_size)<<")";
			ret = false;
			notset_start=-1;
			notset_size += len * 4096;
		}
	}

	if(notset_start!=-1)
	{
		int64_t len = bitmap_size - notset_start;
		XLOG(ERR) << "Freespace check sfs " << db_path << " bits not set start " << std::to_string(notset_start * 4096)<<
			" len " << std::to_string(len*4096) << " ("+folly::prettyPrint(len*4096, folly::PRETTY_BYTES_IEC)+") "<<
			" (" << std::to_string(bitmap_size) << ")";
		notset_size += len * 4096;
		ret = false;
	}

	if(!ret)
	{
		XLOG(ERR) << "Freespace check done sfs " << db_path << ". Res: " << std::to_string(ret) << " missing: " << folly::prettyPrint(notset_size, folly::PRETTY_BYTES_IEC);
	}
	else
	{
		XLOG(INFO) << "Freespace check done sfs " << db_path << ". Res: " << std::to_string(ret);
	}

	return ret;
}

bool SingleFileStorage::clear_freespace_cache(MDB_txn* txn)
{
	THREAD_ID tid = gettid();
	MDB_dbi local_dbi_free, local_dbi_free_len;

	int rc = mdb_dbi_open(txn, "free", MDB_CREATE, &local_dbi_free);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening local free dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening local free dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, local_dbi_free, mdb_cmp_varint);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting free comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	rc = mdb_dbi_open(txn, "free_len", MDB_CREATE | MDB_DUPSORT, &local_dbi_free_len);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error opening free_len dbi handle (" + (std::string)mdb_strerror(rc) + ")");
	}
	if (has_mmap_read_error_reset(tid))
	{
		throw std::runtime_error("LMDB: Error opening free_len dbi handle (SIGBUS)");
	}

	rc = mdb_set_compare(txn, local_dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		throw std::runtime_error("LMDB: Error setting free_len comparison function (" + (std::string)mdb_strerror(rc) + ")");
	}

	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, local_dbi_free, &it_cursor);
	MDB_val tkey;
	MDB_cursor_op op = MDB_FIRST;
	bool ret = true;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in free clear_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			continue;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in free clear_freespace_cache (SIGBUS) sfs " << db_path;
			ret = false;
			continue;
		}

		if (rc != MDB_NOTFOUND)
		{
			rc = mdb_cursor_del(it_cursor, 0);

			if (rc)
			{
				XLOG(ERR) << "Error del in clear_freespace_cache(" << mdb_strerror(rc) << ") sfs " << db_path;
				ret = false;
				continue;
			}

			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "Error del in clear_freespace_cache (SIGBUS) sfs " << db_path;
				ret = false;
				continue;
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	mdb_cursor_open(txn, local_dbi_free_len, &it_cursor);
	op = MDB_FIRST;

	do
	{
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in free len clear_freespace_cache (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			continue;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in free len clear_freespace_cache (SIGBUS) sfs " << db_path;
			ret = false;
			continue;
		}

		if (rc != MDB_NOTFOUND)
		{
			rc = mdb_cursor_del(it_cursor, 0);

			if (rc)
			{
				XLOG(ERR) << "Error del len in clear_freespace_cache("  << mdb_strerror(rc) << ") sfs " << db_path;
				ret = false;
				continue;
			}

			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "Error del len in clear_freespace_cache (SIGBUS) sfs " << db_path;
				ret = false;
				continue;
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return ret;
}


std::string SingleFileStorage::compress_filename(const std::string & fn)
{
	assert(false);
	std::string key;
	size_t slash = fn.find_last_of('/');
	if (slash != std::string::npos)
	{
		key = fn.substr(slash + 1);
	}
	else
	{
		key = fn;
	}

	size_t hash_pos = key.find_first_of('#');

	if (hash_pos == std::string::npos)
	{
		int64_t transid = std::atoll(getuntil("_", key).c_str());
		std::string hex = getafter("_", key);
		if (hex.find("_") != std::string::npos)
		{
			int64_t clouddrive_id = transid;
			transid = std::atoll(getuntil("_", hex).c_str());
			hex = getafter("_", hex);

			if (!isHex(hex))
			{
				return fn;
			}

			std::string nameb = folly::unhexlify(hex);

			if (std::to_string(clouddrive_id) + "_" + std::to_string(transid) + "_" + folly::hexlify(nameb) != key)
			{
				return fn;
			}

			CWData compr;
			compr.addChar(2);
			compr.addVarInt(clouddrive_id);
			compr.addVarInt(transid);
			compr.addString2(nameb.data());

			return std::string(compr.getDataPtr(), compr.getDataSize());
		}

		if (!isHex(hex))
		{
			return fn;
		}

		std::string nameb = folly::unhexlify(hex);

		if (std::to_string(transid) + "_" + folly::hexlify(nameb) != key)
		{
			return fn;
		}

		CWData compr;
		compr.addChar(0);
		compr.addVarInt(transid);
		compr.addString2(nameb);

		return std::string(compr.getDataPtr(), compr.getDataSize());
	}
	else
	{
		int64_t disk_id = std::atoll(key.substr(0, hash_pos).c_str());

		if (disk_id == 0)
		{
			return fn;
		}

		key = key.substr(hash_pos + 1);

		int64_t transid = std::atoll(getuntil("_", key).c_str());
		std::string hex = getafter("_", key);

		if (hex.find("_") != std::string::npos)
		{
			int64_t clouddrive_id = transid;
			transid = std::atoll(getuntil("_", hex).c_str());
			hex = getafter("_", hex);

			if (!isHex(hex))
			{
				return fn;
			}

			std::string nameb = folly::unhexlify(hex);

			if (std::to_string(clouddrive_id) + "_" + std::to_string(transid) + "_" + folly::hexlify(nameb) != key)
			{
				return fn;
			}

			CWData compr;
			compr.addChar(3);
			compr.addVarInt(disk_id);
			compr.addVarInt(clouddrive_id);
			compr.addVarInt(transid);
			compr.addString2(nameb.data());

			return std::string(compr.getDataPtr(), compr.getDataSize());
		}

		if (!isHex(hex))
		{
			return fn;
		}

		std::string nameb = folly::unhexlify(hex);

		if (std::to_string(transid) + "_" + folly::hexlify(nameb) != key)
		{
			return fn;
		}

		CWData compr;
		compr.addChar(1);
		compr.addVarInt(disk_id);
		compr.addVarInt(transid);
		compr.addString2(nameb.data());

		return std::string(compr.getDataPtr(), compr.getDataSize());
	}
}

std::string SingleFileStorage::decompress_filename(const std::string & fn)
{
	if (fn.empty() || (fn[0] != 0 && fn[0] != 1 && fn[0] != 2 && fn[0] != 3) )
	{
		return fn;
	}

	if (fn[0] == 0)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		return std::to_string(transid) + "_" + folly::hexlify(nameb);
	}
	else if(fn[0]==1)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t disk_id;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&disk_id)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		return std::to_string(disk_id) + "#"+ std::to_string(transid) + "_" + folly::hexlify(nameb);
	}
	else if (fn[0] == 2)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t clouddrive_id;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&clouddrive_id)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		return std::to_string(clouddrive_id) + "_" + std::to_string(transid) + "_" + folly::hexlify(nameb);
	}
	else if (fn[0] == 3)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t disk_id;
		int64_t clouddrive_id;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&disk_id)
			|| !rdata.getVarInt(&clouddrive_id)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		return std::to_string(disk_id) + "#" + std::to_string(clouddrive_id) + "_" + std::to_string(transid) + "_" + folly::hexlify(nameb);
	}
	else
	{
		abort();
	}
}

int64_t SingleFileStorage::get_fn_disk_id(const std::string & fn)
{
	if (fn.empty())
	{
		return 0;
	}

	if (fn[0] == 0 || fn[0]==2)
	{
		return 0;
	}

	if (fn[0] != 1
		&& fn[0] != 3)
	{
		return std::atoll(getuntil("#", fn).c_str());
	}

	CRData rdata(fn.data(), fn.size());

	char ch;
	int64_t disk_id;

	if (!rdata.getChar(&ch)
		|| !rdata.getVarInt(&disk_id))
	{
		return 0;
	}

	return disk_id;
}

std::string SingleFileStorage::remove_disk_id(const std::string & fn, size_t disk_id_size)
{
	if (fn.empty())
	{
		return 0;
	}

	if (fn[0] == 0
		|| fn[0] == 2)
	{
		return fn;
	}

	if (fn[0] != 1
		&& fn[0] != 3)
	{
		if (fn.size() > disk_id_size
			&& fn[disk_id_size]=='#')
			return fn.substr(disk_id_size + 1);
		else
			return fn;
	}


	if (fn[0] == 1)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t e_disk_id;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&e_disk_id)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		CWData compr;
		compr.addChar(0);
		compr.addVarInt(transid);
		compr.addString2(nameb);

		return std::string(compr.getDataPtr(), compr.getDataSize());
	}
	else if(fn[0]==3)
	{
		CRData rdata(fn.data(), fn.size());

		char ch;
		int64_t e_disk_id;
		int64_t clouddrive_id;
		int64_t transid;
		std::string nameb;

		if (!rdata.getChar(&ch)
			|| !rdata.getVarInt(&e_disk_id)
			|| !rdata.getVarInt(&clouddrive_id)
			|| !rdata.getVarInt(&transid)
			|| !rdata.getStr2(&nameb))
		{
			return fn;
		}

		CWData compr;
		compr.addChar(2);
		compr.addVarInt(clouddrive_id);
		compr.addVarInt(transid);
		compr.addString2(nameb);

		return std::string(compr.getDataPtr(), compr.getDataSize());
	}
	else
	{
		abort();
	}
}

class Buckets
{
public:
	Buckets()
		:curr_bucket_div(4096) {}

	void add(int64_t len)
	{
		if (len%curr_bucket_div != 0)
		{
			len /= curr_bucket_div;
			++len;
		}
		else
		{
			len /= curr_bucket_div;
		}

		++buckets[len];

		if (buckets.size() > 40)
		{
			rebuild();
		}
	}

	std::string hist()
	{
		std::string ret;

		for (auto &it : buckets)
		{
			ret += folly::prettyPrint(it.first*curr_bucket_div, folly::PRETTY_BYTES_IEC) + ": " + std::to_string(it.second) + " exts\n";
		}

		return ret;
	}

private:
	void rebuild()
	{
		curr_bucket_div *= 2;

		std::map<int64_t, int64_t> new_buckets;
		for (auto& it : buckets)
		{
			int64_t nval;
			if (it.first % 2 == 0)
			{
				nval /= 2;
			}
			else
			{
				nval /= 2;
				++nval;
			}
			new_buckets[it.first / 2] += it.second;
		}

		buckets = new_buckets;
	}

	std::map<int64_t, int64_t> buckets;
	int64_t curr_bucket_div;
};


std::string SingleFileStorage::freespace_stats()
{

	MDB_txn* txn;
	int rc = mdb_txn_begin(cache_db_env!=nullptr ? cache_db_env : db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return std::string();
	}

	rc = mdb_set_compare(txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error setting compare func (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return std::string();
	}

	Buckets buckets;

	int64_t prev_offset = -1;
	int64_t prev_len;
	int64_t total_len = 0;
	int64_t total_count = 0;

	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap freespace_stats (" << mdb_strerror(rc) << ") sfs " << db_path;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen))
			{
				assert(dkey.getStreampos() == dkey.getSize());
				assert(dval.getStreampos() == dval.getSize());

				buckets.add(clen);
				total_len += clen;
				++total_count;

				assert(prev_offset == -1 || prev_offset + prev_len < cstart);

				prev_offset = cstart;
				prev_len = clen;

			}
			else
			{
				assert(false);
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	mdb_txn_abort(txn);

	if (total_count == 0)
		total_count = 1;

	std::string total_str = "Total: " + folly::prettyPrint(total_len, folly::PRETTY_BYTES_IEC) + " " + std::to_string(total_count) + " exts. Avg size " + folly::prettyPrint(static_cast<double>(total_len) / total_count, folly::PRETTY_BYTES_IEC)+"\n";

	return "#### "+ db_path + " #####\n"+ buckets.hist() + total_str + "\n";
}

void SingleFileStorage::wait_for_startup_finish()
{
	std::unique_lock lock(mutex);

	wait_startup_finished(lock);
}

SingleFileStorage::SFragInfo SingleFileStorage::get_frag_info(MDB_txn* txn, const std::string_view fn, bool parse_data, const bool read_newest)
{
	THREAD_ID tid = gettid();

	bool tear_down_txn = false;
	if (txn == nullptr)
	{
		tear_down_txn = true;

		setup_mmap_read_error(tid);

		int rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &txn);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Error starting transaction for read (" << mdb_strerror(rc) << ") txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
			clear_mmap_read_error(tid);
			return SFragInfo();
		}

		if(key_compare_func)
		{
			rc = mdb_set_compare(txn, dbi_main, key_compare_func);
			if (rc)
			{
				XLOG(ERR) << "LMDB: Error setting main compare func in get_frag_info (" << mdb_strerror(rc) << ") sfs " << db_path;
				return SFragInfo();
			}
		}
	}

	SCOPE_EXIT {
		if(tear_down_txn)
			mdb_txn_abort(txn);
	};

	MDB_val tkey;
	tkey.mv_data = const_cast<char*>(&fn[0]);
	tkey.mv_size = fn.size();

	int rc;
	MDB_val tvalue;
	MDB_cursor* mc = nullptr;

	SCOPE_EXIT { mdb_cursor_close(mc); };

	if(read_newest)
	{
		rc = mdb_cursor_open(txn, dbi_main, &mc);

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed mdb_cursor_open in get_frag_info (" << mdb_strerror(rc) << ") sfs " << db_path;
			return SFragInfo();
		}
		
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed  mdb_cursor_open in get_frag_info (SIGBUS) sfs " << db_path;
			return SFragInfo();
		}

		rc = mdb_cursor_get(mc, &tkey, &tvalue, MDB_SET_RANGE);
	}
	else
	{
		rc = mdb_get(txn, dbi_main, &tkey, &tvalue);
	}

	if (tear_down_txn)
	{
		if (clear_mmap_read_error(tid))
		{
			XLOG(ERR) << "LMDB: Error getting fragment info because of SIGBUS txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
			mdb_txn_abort(txn);
			return SFragInfo();
		}
	}
	else
	{
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting fragment info because of SIGBUS txn = " << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
			return SFragInfo();
		}
	}

	if (rc)
	{
		if (rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting fragment info (" << mdb_strerror(rc) << ") txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
		}
		return SFragInfo();
	}

	SFragInfo ret;
	if(read_newest)
	{
		ret.fn = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
		if(common_prefix_func(ret.fn) != common_prefix_func(fn))
		{
			return SFragInfo();
		}
	}

	if(!parse_data)
	{
		ret.offset = -2;
		return ret;
	}

	CRData rdata(reinterpret_cast<char*>(tvalue.mv_data), tvalue.mv_size);

	if (!rdata.getVarInt(&ret.offset)
		|| !rdata.getVarInt(&ret.len))
	{
		XLOG(ERR) << "LMDB: Error extracting offset and len from fragment info txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
		ret = SFragInfo();
	}
	else
	{
		if(!read_extra_exts(ret.offset, rdata, ret.extra_exts))
		{
			XLOG(ERR) << "LMDB: Error reading extra exts from fragment info txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
			ret = SFragInfo();
		}

		if (ret.offset != -1)
		{
			if(!rdata.getVarInt(&ret.last_modified) ||
				!rdata.getStr2(&ret.md5sum) )
			{
				XLOG(WARN) << "LMDB: Error reading last_modified, md5sum txn=" << std::to_string(reinterpret_cast<int64_t>(txn)) << " sfs " << db_path;
			}
		}		
	}

	return ret;
}

bool SingleFileStorage::generate_free_len_idx(MDB_txn * txn)
{
	THREAD_ID tid = gettid();
	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	int rc;
	bool ret = true;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (SIGBUS) sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen)
				&& dkey.getStreampos() == dkey.getSize()
				&& dval.getStreampos() == dval.getSize())
			{
				rc = mdb_put(txn, dbi_free_len, &tval, &tkey, 0);

				if (rc)
				{
					XLOG(ERR) << "Error adding freemap idx item (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}

				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "Error adding freemap idx item (SIGBUS) sfs " << db_path;
					ret = false;
					break;
				}
			}
			else
			{
				XLOG(WARN) << "Deleting invalid freemap extent sfs " << db_path;
				mdb_cursor_del(it_cursor, 0);
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return ret;
}

int64_t SingleFileStorage::get_disk_id(MDB_txn* txn, THREAD_ID tid, const std::string & uuid)
{
	MDB_val val;
	val.mv_data = const_cast<char*>(uuid.data());
	val.mv_size = uuid.size();

	MDB_val disk_id_out;

	int rc = mdb_get(txn, dbi_size, &val, &disk_id_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Error getting data file max size (" << mdb_strerror(rc) << ")";
		return 0;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Error getting data file max size (SIGBUS)";
		return 0;
	}

	int64_t ret = 0;
	if (rc != MDB_NOTFOUND
		&& disk_id_out.mv_size==sizeof(int64_t))
	{
		memcpy(&ret, disk_id_out.mv_data, sizeof(ret));
	}

	if (ret==0)
	{
		ret = next_disk_id;
		++next_disk_id;

		MDB_val tkey;
		char ch = 1;
		tkey.mv_data = &ch;
		tkey.mv_size = 1;

		MDB_val tval;

		tval.mv_data = &next_disk_id;
		tval.mv_size = sizeof(next_disk_id);

		rc = mdb_put(txn, dbi_size, &tkey, &tval, 0);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to put next_disk_id in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
			return 0;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to put next_disk_id in commit (SIGBUS) sfs " << db_path;
			return 0;
		}

		tkey.mv_data = const_cast<char*>(uuid.data());
		tkey.mv_size = uuid.size();

		tval.mv_data = &ret;
		tval.mv_size = sizeof(ret);

		rc = mdb_put(txn, dbi_size, &tkey, &tval, 0);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to put next_disk_id in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
			return 0;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to put next_disk_id in commit (SIGBUS) sfs " << db_path;
			return 0;
		}
	}

	return ret;
}

int64_t SingleFileStorage::get_disk_trans_id(MDB_txn * txn, THREAD_ID tid, int64_t disk_id)
{
	MDB_val val;
	val.mv_data = &disk_id;
	val.mv_size = sizeof(disk_id);

	MDB_val disk_id_out;

	int rc = mdb_get(txn, dbi_size, &val, &disk_id_out);

	if (rc && rc != MDB_NOTFOUND)
	{
		XLOG(ERR) << "LMDB: Error getting disk trans id (" << mdb_strerror(rc) << ")";
		return -1;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Error getting disk trans id (SIGBUS)";
		return -1;
	}

	if (rc != MDB_NOTFOUND
		&& disk_id_out.mv_size == sizeof(int64_t))
	{
		int64_t ret;
		memcpy(&ret, disk_id_out.mv_data, sizeof(ret));
		return ret;
	}
	else if (rc == MDB_NOTFOUND)
	{
		return 0;
	}

	return -1;
}

bool SingleFileStorage::set_disk_trans_id(MDB_txn * txn, THREAD_ID tid, int64_t disk_id, int64_t trans_id)
{
	MDB_val tkey;
	tkey.mv_data = &disk_id;
	tkey.mv_size = sizeof(disk_id);

	MDB_val tval;
	tval.mv_data = &trans_id;
	tval.mv_size = sizeof(trans_id);

	int rc = mdb_put(txn, dbi_size, &tkey, &tval, 0);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to put disk trans id in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to put disk trans id in commit (SIGBUS) sfs " << db_path;
		return false;
	}

	return true;
}

bool SingleFileStorage::rewrite_npages(MDB_txn* txn, MDB_cursor* mc, THREAD_ID tid, size_t npages)
{
	for (size_t i = 0; i < npages; ++i)
	{
		size_t pgno;
		int rc = mdb_cursor_next_leaf_page(mc, &pgno);

		if (rc == MDB_NOTFOUND)
			return true;

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed mdb_cursor_next_leaf_page in rewrite_npages (" << mdb_strerror(rc) << ") sfs " << db_path;
			return false;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed mdb_cursor_next_leaf_page in rewrite_npages (SIGBUS) sfs " << db_path;
			return false;
		}

		int dirty;
		if (mdb_page_is_dirty(mc, pgno, &dirty) == MDB_SUCCESS &&
			!dirty)
		{
			MDB_val tkey;
			MDB_val tval;
			rc = mdb_cursor_get(mc, &tkey, &tval, MDB_GET_CURRENT);

			if (rc)
			{
				XLOG(ERR) << "LMDB: Failed mdb_cursor_get in rewrite_npages (" << mdb_strerror(rc) << ") sfs " << db_path;
				return false;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Failed mdb_cursor_get in rewrite_npages (SIGBUS) sfs " << db_path;
				return false;
			}

			rc = mdb_cursor_put(mc, &tkey, &tval, MDB_CURRENT);

			if (rc)
			{
				XLOG(ERR) << "LMDB: Failed mdb_cursor_put in rewrite_npages (" << mdb_strerror(rc) << ") sfs " << db_path;
				return false;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Failed mdb_cursor_put in rewrite_npages (SIGBUS) sfs " << db_path;
				return false;
			}
		}
	}

	return true;
}

int SingleFileStorage::put_with_rewrite(MDB_txn* txn, MDB_dbi dbi, MDB_val* tkey, MDB_val* tval, THREAD_ID tid, size_t npages)
{
	MDB_cursor* mc;
	int rc = mdb_cursor_open(txn, dbi, &mc);

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed mdb_cursor_open in put_with_rewrite (" << mdb_strerror(rc) << ") sfs " << db_path;
		return rc;
	}

	SCOPE_EXIT { mdb_cursor_close(mc); };

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed  mdb_cursor_open in put_with_rewrite (SIGBUS) sfs " << db_path;
		return rc;
	}

	rc = mdb_cursor_put(mc, tkey, tval, 0);

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed mdb_cursor_open in put_with_rewrite (" << mdb_strerror(rc) << ") sfs " << db_path;
		return rc;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed  mdb_cursor_open in put_with_rewrite (SIGBUS) sfs " << db_path;
		return rc;
	}

	if (with_rewrite)
		rewrite_npages(txn, mc, tid, npages);

	return MDB_SUCCESS;
}

void SingleFileStorage::operator()()
{
	int64_t curr_write_ext_start;
	int64_t curr_write_ext_end;
	{
		std::scoped_lock lock(datafileoffset_mutex);
		curr_write_ext_start = data_file_offset;
		curr_write_ext_end = data_file_offset_end;
	}

	THREAD_ID tid = gettid();
	setup_mmap_read_error(tid);

	int64_t commit_errors = 0;
	MDB_txn* txn;
	int rc = mdb_txn_begin(db_env, NULL, 0, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (1) (SIGBUS) sfs " << db_path;
		++commit_errors;
	}

	XLOG(INFO) << "Initial txn id " << std::to_string(mdb_get_txnid(txn));

	MDB_txn* freespace_txn;

	if (cache_db_env != nullptr)
	{
		rc = mdb_txn_begin(cache_db_env, NULL, 0, &freespace_txn);

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to open transaction handle for cache_db commit (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to open transaction handle for cache_db commit (1) (SIGBUS) sfs " << db_path;
			++commit_errors;
		}
		XLOG(INFO) << "Using seperate freespace cache";

		rc = mdb_dbi_open(freespace_txn, "free", 0, &dbi_free);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to open free dbi cache_db commit (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to open free dbi for cache_db commit (1) (SIGBUS) sfs " << db_path;
			++commit_errors;
		}

		rc = mdb_dbi_open(freespace_txn, "free_len", 0, &dbi_free_len);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to open free len dbi cache_db commit (1) (" << (std::string)mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to open free len dbi for cache_db commit (1) (SIGBUS) sfs " << db_path;
			++commit_errors;
		}

		rc = mdb_dbi_open(freespace_txn, "size", 0, &dbi_cache_size);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to open cache size dbi cache_db commit (1) (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to open cache size dbi for cache_db commit (1) (SIGBUS) sfs " << db_path;
			++commit_errors;
		}

		rc = mdb_txn_commit(freespace_txn);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to commit freespace_txn beg (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to commit freespace_txn beg (SIGBUS) sfs " << db_path;
			++commit_errors;
		}

		rc = mdb_txn_begin(cache_db_env, NULL, 0, &freespace_txn);

		if (rc)
		{
			XLOG(ERR) << "LMDB: Failed to open transaction handle for cache_db commit (3) (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed to open transaction handle for cache_db commit (3) (SIGBUS) sfs " << db_path;
			++commit_errors;
		}
	}
	else
	{
		XLOG(INFO) << "Not having seperate freespace cache";
		freespace_txn = txn;
	}

	rc = mdb_set_compare(freespace_txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting free comparison function (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}

	rc = mdb_set_compare(freespace_txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		XLOG(ERR) << "Error setting free_len comparison function (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}

	rc = mdb_set_compare(txn, dbi_holes, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting holes comparison function (" << mdb_strerror(rc) << ") sfs " << db_path;
		++commit_errors;
	}

	if(key_compare_func)
	{
		rc = mdb_set_compare(txn, dbi_main, key_compare_func);
		if (rc)
		{
			XLOG(ERR) << "Error setting main comparison function (" << mdb_strerror(rc) << ") sfs " << db_path;
			++commit_errors;
		}	
	}

	if (!regen_freespace_cache
		&& cache_db_env == nullptr
		&& std::filesystem::exists("/etc/urbackup/regen_free_space_cache"))
	{
		regen_freespace_cache = true;

		XLOG(WARN) <<" Clearing main freespace cache (manual regen)..."; 

		if (!clear_freespace_cache(txn))
		{
			XLOG(ERR) << "Error clearing main freespace cache";
			++commit_errors;
		}
	}

	if (regen_freespace_cache)
	{
		data_file_free=0;
		if (!generate_freespace_cache(txn, freespace_txn, false))
		{
			XLOG(ERR) << "Generating freespace cache failed";

			if (cache_db_env == nullptr)
			{
				abort();
			}

			++commit_errors;
		}
	}

	if (std::filesystem::exists("/etc/urbackup/check_free_space_cache") )
	{
		if (!freespace_check(txn, freespace_txn, true))
		{
			XLOG(ERR) << "Freespace check failed";
			++commit_errors;
		}
	}

	if (regen_freespace_cache
		&& cache_db_env!=nullptr
		&& commit_errors == 0)
	{
		XLOG(INFO) << "Clearing main freespace cache...";

		if (!clear_freespace_cache(txn))
		{
			XLOG(ERR) << "Error clearing main freespace cache";
			++commit_errors;
		}
	}

	{
		char ch = dbi_size_info_ext_freespace;
		MDB_val key;
		key.mv_data = &ch;
		key.mv_size = 1;

		if (cache_db_env == nullptr)
		{
			XLOG(INFO) << "Ext freespace disabled";
			rc = mdb_del(txn, dbi_size, &key, NULL);

			if (rc && rc!=MDB_NOTFOUND)
			{
				XLOG(ERR) << "LMDB: Failed to mdb_del dbi_size_info_ext_freespace (" << mdb_strerror(rc) << ") sfs " << db_path;
				++commit_errors;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Failed to mdb_del dbi_size_info_ext_freespace (SIGBUS) sfs " << db_path;
				++commit_errors;
			}
		}
		else
		{
			int64_t ival = sync_freespace_cache ? 1 : 0;

			if(sync_freespace_cache)
			{
				XLOG(INFO) << "Ext freespace sync";
			}
			else
			{
				XLOG(INFO) << "Ext freespace nosync";
			}

			MDB_val val;
			val.mv_data = &ival;
			val.mv_size = sizeof(ival);

			rc = mdb_put(txn, dbi_size, &key, &val, 0);

			if (rc)
			{
				XLOG(ERR) << "LMDB: Failed to mdb_put dbi_size_info_ext_freespace (" << mdb_strerror(rc) << ") sfs " << db_path;
				++commit_errors;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Failed to mdb_put dbi_size_info_ext_freespace (SIGBUS) sfs " << db_path;
				++commit_errors;
			}
		}
	}

	if (data_file_free == 0)
	{
		if(!regen_datafile_free(freespace_txn))
		{
			++commit_errors;
		}
	}

	if(!regen_free_len_idx(freespace_txn))
	{
		++commit_errors;
	}

	if (std::filesystem::exists("/etc/urbackup/sfs_delete_holes"))
	{
		commit_errors += reset_holes(txn, freespace_txn, tid);
	}

	std::unique_lock lock(mutex);

	commit_errors += reset_del_log_fn(txn, freespace_txn, tid, 0, curr_transid);

	commit_errors += reset_del_queue(txn, freespace_txn, tid, 0, curr_transid);

	startup_finished = true;

	lock.unlock();

	if (!do_free_minspace(txn, freespace_txn, tid))
	{
		XLOG(ERR) << "Freeing minspace failed";
		++commit_errors;
	}

	lock.lock();

	size_t mod_items = 0;

	while (!do_quit
		&& !is_dead)
	{
		if (commit_errors > 0)
		{
			write_offline = true;
			do_stop_on_error();
		}

		int first_wait = 0;
		while (commit_queue.empty()
			&& (commit_background_queue.empty() || mdb_curr_sync )
			&& !do_quit)
		{
			if (first_wait == 0)
			{
				first_wait = 1;
				cond.wait_for(lock, 10s);
			}
			else if (first_wait == 1)
			{
				lock.unlock();
				first_wait = 2;
				//TODO: Server->mallocFlushTcache();
				lock.lock();
			}
			else
			{
				cond.wait(lock);
			}
		}

		if (do_quit
			|| is_dead)
		{
			break;
		}

		SFragInfo frag_info;
		/*if (mod_items > 100000)
		{
			frag_info.action = FragAction::Commit;
			mod_items = 0;
		}
		else
		{*/
			if (commit_queue.empty())
			{
				assert(!commit_background_queue.empty());
				frag_info = commit_background_queue.front();
				commit_background_queue.pop_front();
			}
			else
			{
				frag_info = commit_queue.front();
				commit_queue.pop_front();
			}
		//}

		if (frag_info.action == FragAction::Commit
			&& is_defragging)
		{
			XLOG(INFO) <<"Waiting for defrag to restart before commiting...";
			defrag_restart = 1;
			bool postpone_commit = false;
			while (defrag_restart==1
				&& is_defragging
				&& !postpone_commit)
			{
				lock.unlock();
				std::this_thread::sleep_for(100ms);
				lock.lock();

				//Reorder getting free extent -- otherwise there might be deadlocks
				for (auto it=commit_queue.begin();it!=commit_queue.end();++it)
				{
					if (it->action == FragAction::FindFree
						&& it->len == 1)
					{
						XLOG(WARN) << "Found getting free extent during waiting for defrag to restart for commit. Getting free exitent first...";
						postpone_commit = true;
						commit_queue.push_front(frag_info);
						frag_info = *it;
						commit_queue.erase(it);
						break;
					}
				}
			}
		}
		else if (frag_info.action == FragAction::Commit)
		{
			defrag_restart = 0;
		}

		lock.unlock();

		if (frag_info.action == FragAction::Commit
			&& frag_info.offset != -1)
		{
			int64_t disk_id = frag_info.len;
			if (disk_id == 0)
				curr_transid = frag_info.offset;
			else
				set_disk_trans_id(txn, tid, disk_id, frag_info.offset);
		}

		if (frag_info.action == FragAction::Add
			|| frag_info.action == FragAction::AddNoDelOld
			|| frag_info.action == FragAction::Del
			|| frag_info.action == FragAction::DelOld
			|| frag_info.action == FragAction::RestoreOld
			|| frag_info.action==FragAction::DelWithQueued)
		{
			++mod_items;
			--commit_items[common_prefix_hash_func(frag_info.fn)];
		}

		if (frag_info.action == FragAction::FindFree
			|| frag_info.action == FragAction::Del
			|| frag_info.action == FragAction::DelWithQueued)
		{
			mod_items += 2;
		}

		if (frag_info.action == FragAction::Commit )
		{
			if (frag_info.md5sum == "reset"
				&& commit_errors>0)
			{
				XLOG(ERR) << "Resetting commit errors (" << std::to_string(commit_errors) << ") by resetting (abort+begin) transaction";

				mdb_txn_abort(txn);
				if (freespace_txn == txn)
				{
					freespace_txn = nullptr;
				}
				txn = nullptr;

				commit_errors = 0;

				int rc = mdb_txn_begin(db_env, NULL, 0, &txn);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (2) (SIGBUS) sfs " << db_path;
					++commit_errors;
				}

				if (cache_db_env == nullptr)
				{
					freespace_txn = txn;
				}
				else
				{
					mdb_txn_abort(freespace_txn);
					freespace_txn = nullptr;

					int rc = mdb_txn_begin(cache_db_env, NULL, 0, &freespace_txn);
					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to open transaction handle for freespace commit (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
						++commit_errors;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to open transaction handle for freespace commit (2) (SIGBUS) sfs " << db_path;
						++commit_errors;
					}
				}

				if(!setup_compare_funcs(txn, freespace_txn))
				{
					++commit_errors;
				}

				commit_errors += reset_del_queue(txn, freespace_txn, tid, 0, curr_transid);
			}

			if (curr_new_free_extents.size() < 1000 ||
				std::filesystem::exists("/var/urbackup/sfs_always_run_do_free_minspace") )
			{
				if (!do_free_minspace(txn, freespace_txn, tid))
				{
					XLOG(ERR) << "Freeing minspace failed (2)";
					++commit_errors;
				}
			}

			if (data_file.fsyncNoInt()!=0)
			{
				XLOG(ERR) << "Failed to sync data file " << data_file_path << ". " << folly::errnoStr(errno);
				++commit_errors;
			}

			if (new_data_file
				&& new_data_file.fsyncNoInt()!=0)
			{
				XLOG(ERR) << "Failed to sync new data file " << (data_file_path.parent_path() / "new_data") << ". " << folly::errnoStr(errno);
				++commit_errors;
			}

			size_t num_reserved_extents;
			int64_t local_curr_partid;
			int64_t local_curr_version;
			{
				std::vector<SPunchItem> local_reserved_extents;
				{
					std::scoped_lock lock(reserved_extents_mutex);
					local_curr_partid = curr_partid;
					local_curr_version = curr_version;
					local_reserved_extents.reserve(reserved_extents.size());
					for(const auto& it: reserved_extents)
					{
						if( it.first + it.second <= data_file_max_size &&
							 !(it.first >= curr_write_ext_start && 
								it.first + it.second <= curr_write_ext_end) )
							local_reserved_extents.emplace_back(SPunchItem(it.first, it.second));
					}
				}

				num_reserved_extents = local_reserved_extents.size();
			
				size_t ext_idx = 0;
				for(const auto& reserved_extent: local_reserved_extents)
				{
					commit_errors += add_tmp(ext_idx, txn, tid, reserved_extent.offset, reserved_extent.len);
					++ext_idx;

					auto size = reserved_extent.offset + div_up(reserved_extent.len, block_size)*block_size;

					if (size > curr_write_ext_start
						&& size <= curr_write_ext_end)
						curr_write_ext_start = size;
					if (size > data_file_max_size)
						data_file_max_size = size;
				}
			}


			if (curr_write_ext_start != -1)
			{
				MDB_val tkey;
				char ch = dbi_size_info_size;
				tkey.mv_data = &ch;
				tkey.mv_size = 1;

				MDB_val tval;
				int64_t tdata[7];

				tdata[0] = data_file_max_size;
				tdata[1] = curr_write_ext_start;
				tdata[2] = curr_write_ext_end;
				tdata[3] = data_file_free;
				tdata[4] = curr_transid;
				tdata[5] = local_curr_partid;
				tdata[6] = local_curr_version;

				tval.mv_data = tdata;
				tval.mv_size = sizeof(tdata);

				rc = mdb_put(txn, dbi_size, &tkey, &tval, 0);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to put max size in commit (" << mdb_strerror(rc) << ") sfs "<<db_path;
					++commit_errors;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to put max size in commit (SIGBUS) sfs " << db_path;
					++commit_errors;
				}
			}
			else
			{
				XLOG(ERR) << "curr_write_ext_start=-1 sfs " << db_path;
				++commit_errors;
			}

			int64_t new_data_file_copy_done = 0;
			{
				std::scoped_lock copy_lock(data_file_copy_mutex);
				if (data_file_copy_done > 0)
				{
					MDB_val tkey;
					char ch = 0;
					tkey.mv_data = &ch;
					tkey.mv_size = dbi_size_info_migration;

					MDB_val tval;
					tval.mv_data = &data_file_copy_done;
					tval.mv_size = sizeof(data_file_copy_done);

					rc = mdb_put(txn, dbi_size, &tkey, &tval, 0);
					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to put data_file_copy_done in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
						++commit_errors;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to put data_file_copy_done in commit (SIGBUS) sfs " << db_path;
						++commit_errors;
					}

					if (commit_errors == 0)
					{
						new_data_file_copy_done = data_file_copy_done;
					}
				}
			}

			if (commit_errors > 0)
			{
				write_offline = true;
				do_stop_on_error();
			}

			bool commit_ok = true;
			if (!write_offline)
			{
				bool commit_freespace = true;
				if (sync_freespace_cache
					&& cache_db_env!=nullptr
					&& commit_freespace)
				{
					MDB_val main_txnid_key;
					char ch = dbi_size_info_size;
					main_txnid_key.mv_data = &ch;
					main_txnid_key.mv_size = 1;

					MDB_val main_txnid_val;
					int64_t main_txnid = mdb_get_txnid(txn);
					main_txnid_val.mv_data = &main_txnid;
					main_txnid_val.mv_size = sizeof(main_txnid);

					rc = mdb_put(freespace_txn, dbi_cache_size,
						&main_txnid_key, &main_txnid_val, 0);

					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to put main txnid (" << mdb_strerror(rc) << ") sfs " << db_path;
						++commit_errors;
						commit_ok = false;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to put main txnid (SIGBUS) sfs " << db_path;
						++commit_errors;
						commit_ok = false;
					}

					rc = mdb_txn_commit(freespace_txn);

					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to commit freespace (" << mdb_strerror(rc) << ") sfs " << db_path;
						++commit_errors;
						commit_ok = false;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to commit freespace (SIGBUS) sfs " << db_path;
						++commit_errors;
						commit_ok = false;
					}

					freespace_txn = nullptr;

					int rc = mdb_txn_begin(cache_db_env, NULL, 0, &freespace_txn);
					if (rc)
					{
						XLOG(ERR) << "LMDB: Failed to open transaction handle for freespace commit (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
						++commit_errors;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Failed to open transaction handle for freespace commit (2) (SIGBUS) sfs " << db_path;
						++commit_errors;
					}
				}

				rc = mdb_txn_commit(txn);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to commit (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
					commit_ok = false;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to commit (SIGBUS) sfs " << db_path;
					++commit_errors;
					commit_ok = false;
				}
				txn = nullptr;

				if (commit_ok)
				{
					auto active_fn = data_file_path.parent_path() / "active";
					if (!std::filesystem::exists(active_fn))
					{
						int fd = open(active_fn.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
						if (fd !=-1)
						{
							folly::fsyncNoInt(fd);
							os_sync(active_fn.parent_path().string());
						}
					}
				}
			}
			else
			{
				commit_ok = false;
			}

			if (commit_ok)
			{
				curr_new_free_extents.clear();

				if (new_data_file_copy_done > 0)
				{
					std::scoped_lock copy_lock(data_file_copy_mutex);
					data_file_copy_done_sync = new_data_file_copy_done;
				}
			}
			else
			{
				write_offline = true;
				do_stop_on_error();
			}

			bool has_commit_info = false;
			{
				std::scoped_lock llock(mutex);
				if (frag_info.commit_info != nullptr)
				{
					frag_info.commit_info->commit_errors = commit_errors;
					frag_info.commit_info->commit_done.notify_all();
					has_commit_info = true;
				}

				if (is_defragging)
				{
					XLOG(INFO) << "Defrag ctr incremented. Defrag can continue...";
					defrag_restart=0;
				}
			}

			if (has_commit_info
				&& commit_ok)
			{
				commit_errors = 0;
			}

			if (commit_ok)
			{
				int rc = mdb_txn_begin(db_env, NULL, 0, &txn);
				if (rc)
				{
					XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
					++commit_errors;
				}
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Failed to open transaction handle for commit (2) (SIGBUS) sfs " << db_path;
					++commit_errors;
				}

				if (cache_db_env == nullptr)
					freespace_txn = txn;

				XLOG(INFO) << "After commit txn id " + std::to_string(mdb_get_txnid(txn));

				if(!setup_compare_funcs(txn, freespace_txn))
				{
					++commit_errors;
				}

				for(auto it=commit_items.begin();it!=commit_items.end();)
				{
					if(it->second==0)
					{
						auto it_prev=it;
						++it;
						commit_items.erase(it_prev);
					}
					else
					{
						++it;
					}
				}

				for(size_t ext_idx =0;ext_idx <num_reserved_extents;++ext_idx)
				{
					if(rm_tmp(ext_idx, txn, tid)>0)
						commit_ok = false;
				}
			}
		}
		else if (frag_info.action == FragAction::ResetDelLog)
		{
			int64_t disk_id = frag_info.last_modified;
			int64_t reset_transid = frag_info.offset;

			commit_errors+=reset_del_log_fn(txn, freespace_txn, tid, disk_id, reset_transid);

			if (frag_info.commit_info != nullptr)
			{
				std::scoped_lock llock(mutex);
				frag_info.commit_info->commit_errors = commit_errors;
				frag_info.commit_info->commit_done.notify_all();
			}
		}
		else if (frag_info.action == FragAction::GetDiskId)
		{			
			int64_t disk_id = get_disk_id(txn, tid, frag_info.fn);

			if (disk_id == 0)
				++commit_errors;

			if (frag_info.commit_info != nullptr)
			{
				std::scoped_lock llock(mutex);
				frag_info.commit_info->commit_errors = commit_errors;
				frag_info.commit_info->new_datafile_offset = disk_id;
				frag_info.commit_info->commit_done.notify_all();
			}
		}
		else if (frag_info.action == FragAction::EmptyQueue)
		{
			if (frag_info.commit_info != nullptr)
			{
				std::scoped_lock llock(mutex);
				frag_info.commit_info->commit_errors = 0;
				frag_info.commit_info->commit_done.notify_all();
			}
		}
		else if (frag_info.action == FragAction::ReadFragInfo ||
			frag_info.action == FragAction::ReadFragInfoWithoutParsing)
		{
			if (frag_info.commit_info != nullptr)
			{
				frag_info.commit_info->commit_errors = 0;

				std::scoped_lock llock(mutex);

				if (frag_info.commit_info->frag_info != nullptr)
				{
					*frag_info.commit_info->frag_info = get_frag_info(txn, frag_info.fn, frag_info.action == FragAction::ReadFragInfo, frag_info.offset>0);
				}

				frag_info.commit_info->commit_done.notify_all();
			}
		}
		else if (frag_info.action == FragAction::Del
			|| frag_info.action == FragAction::DelOld
			|| frag_info.action == FragAction::DelWithQueued)
		{
			bool del_old = frag_info.action == FragAction::DelOld;
			commit_errors += remove_fn(frag_info.fn, txn, freespace_txn, true, del_old, tid);

			if (frag_info.action == FragAction::DelWithQueued)
			{
				commit_errors += unqueue_del(frag_info.fn, txn, tid);
			}
		}
		else if (frag_info.action == FragAction::AssertDelQueueEmpty)
		{
			MDB_cursor* it_cursor;
			mdb_cursor_open(txn, dbi_queue_del, &it_cursor);

			SCOPE_EXIT { mdb_cursor_close(it_cursor); };
			MDB_val tkey;
			MDB_val tval;
			rc = mdb_cursor_get(it_cursor, &tkey, &tval, MDB_FIRST);

			if (rc && rc != MDB_NOTFOUND)
			{
				++commit_errors;
			}
			else if(rc!=MDB_NOTFOUND)
			{
				CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);
				int64_t transid = 0;
				rdata.getVarInt(&transid);
				std::string fn = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
				XLOG(ERR) << "Found item " << decompress_filename(fn) << " transid "
					<< std::to_string(transid) << " in del queue. Abort()";
				abort();
			}
			else
			{
				XLOG(INFO) << "Assert del queue empty okay";
			}
		}
		else if (frag_info.action == FragAction::QueueDel)
		{
			int64_t log_transid = curr_transid;

			int64_t disk_id = get_fn_disk_id(frag_info.fn);

			if (disk_id != 0)
			{
				log_transid = get_disk_trans_id(txn, tid, disk_id);

				if (log_transid == -1)
				{
					++commit_errors;
				}
			}

			commit_errors += queue_del(frag_info.fn, txn, tid, log_transid);
		}
		else if (frag_info.action == FragAction::UnqueueDel)
		{
			commit_errors += unqueue_del(frag_info.fn, txn, tid);
		}
		else if (frag_info.action == FragAction::RestoreOld)
		{
			commit_errors += restore_fn(frag_info.fn, txn, freespace_txn, tid);
		}
		else if (frag_info.action == FragAction::Add
			|| frag_info.action == FragAction::AddNoDelOld)
		{
			if (frag_info.action == FragAction::Add)
			{
				commit_errors += remove_fn(frag_info.fn, txn, freespace_txn, false, false, tid);
			}
			else
			{
				int64_t disk_id = get_fn_disk_id(frag_info.fn);

				int64_t log_transid = curr_transid;

				if (disk_id != 0)
				{
					log_transid = get_disk_trans_id(txn, tid, disk_id);

					if (log_transid==-1)
					{
						++commit_errors;
					}
				}

				commit_errors += log_fn(frag_info.fn, txn, tid, log_transid);
			}

			CWData wdata;

			int64_t encoded_first_offset;
			if(frag_info.extra_exts.size() < max_inline_exts)
			{
				encoded_first_offset = encode_num_exts(frag_info.offset, static_cast<int64_t>(frag_info.extra_exts.size()));
			}
			else
			{
				encoded_first_offset = encode_max_num_exts(frag_info.offset);
			}

			wdata.addVarInt(encoded_first_offset);
			wdata.addVarInt(frag_info.len);

			if(frag_info.extra_exts.size() >= max_inline_exts)
			{
				wdata.addVarInt(frag_info.extra_exts.size());
				wdata.addVarInt(0);
			}

			int64_t size = frag_info.offset + div_up(frag_info.len, block_size)*block_size;

			if (size > curr_write_ext_start
				&& size <= curr_write_ext_end)
				curr_write_ext_start = size;
			if (size > data_file_max_size)
				data_file_max_size = size;

			for (const SPunchItem& ext : frag_info.extra_exts)
			{
				wdata.addVarInt(ext.offset);
				wdata.addVarInt(ext.len);
				size = ext.offset + div_up(ext.len, block_size)*block_size;

				if (size > curr_write_ext_start
					&& size <= curr_write_ext_end)
					curr_write_ext_start = size;
				if (size > data_file_max_size)
					data_file_max_size = size;
			}

			{
				std::scoped_lock lock(reserved_extents_mutex);

				auto it_r = reserved_extents.find(frag_info.offset);
				if(it_r!=reserved_extents.end())
					reserved_extents.erase(it_r);

				for (const SPunchItem& ext : frag_info.extra_exts)
				{
					it_r = reserved_extents.find(ext.offset);
					if(it_r!=reserved_extents.end())
						reserved_extents.erase(it_r);
				}
			}

			wdata.addVarInt(frag_info.last_modified);
			wdata.addString2(frag_info.md5sum);

			MDB_val tval;
			tval.mv_data = wdata.getDataPtr();
			tval.mv_size = wdata.getDataSize();

			MDB_val tkey;
			tkey.mv_data = const_cast<char*>(&frag_info.fn[0]);
			tkey.mv_size = frag_info.fn.size();

			rc = put_with_rewrite(txn, dbi_main, &tkey, &tval, tid, n_rewrite_pages);
			if (rc)
			{
				XLOG(ERR) << "LMDB: Failed to put extent info in commit (" << mdb_strerror(rc) << ") sfs " << db_path;
				++commit_errors;
			}
		}
		else if (frag_info.action == FragAction::FindFree)
		{
			if (frag_info.offset > curr_write_ext_start)
				curr_write_ext_start = frag_info.offset;
			if (frag_info.offset > data_file_max_size)
				data_file_max_size = frag_info.offset;

			bool search_for_free_space = true;
			bool add_remaining_ext = true;
			if (curr_write_ext_start == data_file_max_size)
			{
				add_remaining_ext = false;
				int64_t free_space = os_free_space(data_file_path.parent_path().string());
				free_space += get_burn_in_data_size();
				int64_t index_file_size;
				int64_t really_min_space = get_really_min_space(index_file_size);
				if ( (data_file_size_limit<=0 || curr_write_ext_start + alloc_chunk_size < data_file_size_limit) &&
					free_space > min_free_space
					&& free_space > really_min_space)
				{
					curr_write_ext_end = curr_write_ext_start + alloc_chunk_size;
					search_for_free_space = false;
				}
				else
				{
					XLOG(WARN) << "Volume full (" << folly::prettyPrint(free_space, folly::PRETTY_BYTES_IEC) << " free space). Switching to using free space in data file";
				}
			}
			else if (force_freespace_check)
			{
				force_freespace_check = false;

				if (curr_write_ext_end - curr_write_ext_start >= block_size
					&& add_remaining_ext)
				{
					add_freemap_ext(freespace_txn, curr_write_ext_start,
						curr_write_ext_end - curr_write_ext_start, false, tid);
					add_remaining_ext = false;
				}

				int64_t free_space = os_free_space(data_file_path.parent_path().string());
				free_space += get_burn_in_data_size();
				int64_t index_file_size;
				int64_t really_min_space = get_really_min_space(index_file_size);
				if (free_space > min_free_space + alloc_chunk_size
					&& free_space > really_min_space + alloc_chunk_size)
				{
					XLOG(WARN) << "Volume has free space (" << folly::prettyPrint(free_space, folly::PRETTY_BYTES_IEC) << " free space). Switching to appending to data file";
					curr_write_ext_start = data_file_max_size;
					curr_write_ext_end = curr_write_ext_start + alloc_chunk_size;
					search_for_free_space = false;
				}
			}
			
			if (search_for_free_space)
			{
				if (curr_write_ext_end - curr_write_ext_start >= block_size
					&& add_remaining_ext)
				{
					add_freemap_ext(freespace_txn, curr_write_ext_start, 
						curr_write_ext_end - curr_write_ext_start, false, tid);
				}

				int64_t start, len;
				if (find_freemap_ext(freespace_txn, tid, start, len))
				{
					XLOG(DBG) << "Writing to free extent (" << std::to_string(start) << ", " << std::to_string(len) << ")";

					MDB_val tkey;
					CWData wtkey;
					wtkey.addVarInt(start);
					tkey.mv_data = wtkey.getDataPtr();
					tkey.mv_size = wtkey.getDataSize();
					rc = mdb_del(freespace_txn, dbi_free, &tkey, nullptr);

					bool mmap_err = has_mmap_read_error_reset(tid);
					if (mmap_err)
					{
						XLOG(ERR) << "LMDB: Error removing free extent (SIGBUS) sfs " << db_path;
						frag_info.commit_info->commit_errors = 1;
						++commit_errors;
					}

					if (rc)
					{
						XLOG(ERR) << "Error removing free extent (" << mdb_strerror(rc) << ") sfs " << db_path;
						frag_info.commit_info->commit_errors = 1;
					}
					else if(!mmap_err)
					{
						MDB_val tval;
						CWData wtval;
						wtval.addVarInt(len);
						tval.mv_data = wtval.getDataPtr();
						tval.mv_size = wtval.getDataSize();

						rc = mdb_del(freespace_txn, dbi_free_len, &tval, &tkey);

						if (rc)
						{
							XLOG(ERR) << "Error removing free extent (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
							frag_info.commit_info->commit_errors = 1;
						}
						if (has_mmap_read_error_reset(tid))
						{
							XLOG(ERR) << "LMDB: Error removing free extent (2) (SIGBUS) sfs " << db_path;
							frag_info.commit_info->commit_errors = 1;
							++commit_errors;
						}
					}

					{
						std::scoped_lock lock2(freespace_mutex);
						data_file_free -= len;
					}

					curr_write_ext_start = start;
					curr_write_ext_end = start + len;
				}
				else
				{
					int64_t free_space = os_free_space(data_file_path.parent_path().string());
					free_space += get_burn_in_data_size();
					int64_t index_file_size;
					int64_t really_min_space = get_really_min_space(index_file_size);

					if (free_space < min_free_space
						|| free_space < really_min_space)
					{
						XLOG(INFO) << "Could not find free extent and not enough free space (" << folly::prettyPrint(free_space, folly::PRETTY_BYTES_IEC) << ") path "<<data_file_path;
						frag_info.commit_info->commit_errors = LLONG_MAX;
					}
					else
					{
						curr_write_ext_start = data_file_max_size;
						curr_write_ext_end = curr_write_ext_start + alloc_chunk_size;
					}
				}
			}

			std::scoped_lock llock(mutex);
			frag_info.commit_info->new_datafile_offset = curr_write_ext_start;
			frag_info.commit_info->new_datafile_offset_end = curr_write_ext_end;
			frag_info.commit_info->commit_done.notify_all();
		}
		else if (frag_info.action == FragAction::FreeExtents)
		{
			int64_t size = frag_info.offset + div_up(frag_info.len, block_size)*block_size;

			if (size > curr_write_ext_start
				&& size <= curr_write_ext_end)
				curr_write_ext_start = size;
			if (size > data_file_max_size)
				data_file_max_size = size;

			{
				std::scoped_lock lock(reserved_extents_mutex);

				auto it_r = reserved_extents.find(frag_info.offset);
				if(it_r!=reserved_extents.end())
					reserved_extents.erase(it_r);

				for (const SPunchItem& ext : frag_info.extra_exts)
				{
					it_r = reserved_extents.find(ext.offset);
					if(it_r!=reserved_extents.end())
						reserved_extents.erase(it_r);
				}
			}

			if (!add_freemap_ext(freespace_txn, frag_info.offset, frag_info.len, true, tid))
			{
				XLOG(ERR) << "LMDB: Failed to put free extent (0) in FreeExtents sfs " << db_path;
				++commit_errors;
			}
			
			for (SPunchItem& ext : frag_info.extra_exts)
			{
				size = ext.offset + div_up(ext.len, block_size)*block_size;

				if (size > curr_write_ext_start
					&& size <= curr_write_ext_end)
					curr_write_ext_start = size;
				if (size > data_file_max_size)
					data_file_max_size = size;

				if (!add_freemap_ext(freespace_txn, ext.offset, ext.len, true, tid))
				{
					XLOG(ERR) << "LMDB: Failed to put free extent (1) in FreeExtents sfs " << db_path;
					++commit_errors;
				}
			}
		}
		else
		{
			assert(false);
		}

		lock.lock();
	}

	if (is_dead && !do_quit)
	{
		SFragInfo frag_info;
		while (!commit_queue.empty()
			|| !commit_background_queue.empty())
		{
			if (!commit_background_queue.empty())
			{
				frag_info = commit_background_queue.front();
				commit_background_queue.pop_front();
			}
			else
			{
				frag_info = commit_queue.front();
				commit_queue.pop_front();
			}

			if (frag_info.commit_info != nullptr)
			{
				std::scoped_lock llock(mutex);
				frag_info.commit_info->commit_errors = 1;
				frag_info.commit_info->commit_done.notify_all();
			}
		}
		
	}

	while (references > 0)
	{
		std::this_thread::sleep_for(100ms);
	}
}

bool SingleFileStorage::setup_compare_funcs(MDB_txn* txn, MDB_txn* freespace_txn)
{
	bool ret = true;
	int rc = mdb_set_compare(freespace_txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting free comparison function (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	rc = mdb_set_compare(freespace_txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		XLOG(ERR) << "Error setting free_len comparison function (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	rc = mdb_set_compare(txn, dbi_holes, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting holes comparison function (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	if(key_compare_func)
	{
		rc = mdb_set_compare(txn, dbi_main, key_compare_func);
		if (rc)
		{
			XLOG(ERR) << "Error setting main comparison function (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
		}
	}

	rc = mdb_set_compare(txn, dbi_new_objects, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "Error setting dbi_new_objects comparison function (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
		ret = false;
	}

	return ret;
}

int64_t SingleFileStorage::get_free_space_in_data_file()
{
	int64_t curr_ext_add = 0;
	{
		std::scoped_lock lock(datafileoffset_mutex);
		if (data_file_offset_end > 0
			&& data_file_offset_end <= div_up(data_file.size(), block_size)*block_size)
		{
			curr_ext_add = data_file_offset_end - data_file_offset;
		}
	}

	std::scoped_lock lock(freespace_mutex);
	return data_file_free + curr_ext_add;
}

int64_t SingleFileStorage::get_free_space_real()
{
	int64_t free_space = os_free_space(data_file_path.parent_path().string());
	free_space += get_burn_in_data_size();

	if (free_space < min_free_space)
		free_space = 0;
	else
		free_space -= min_free_space;

	int64_t data_file_free_space = get_free_space_in_data_file();

	if(data_file_free_space>0)
		data_file_free_space = (data_file_free_space * 9) / 10;

	free_space += (std::max)((int64_t)0, data_file_free_space);

	return free_space;
}

int64_t SingleFileStorage::get_total_space()
{
	int64_t total_space = os_total_space(data_file_path.parent_path().string());
	return total_space;
}

int64_t SingleFileStorage::get_data_file_size()
{
	return data_file.size();
}

int64_t SingleFileStorage::max_free_extent(int64_t& len)
{
	THREAD_ID tid = gettid();
	setup_mmap_read_error(tid);

	MDB_txn* txn;
	int rc = mdb_txn_begin(cache_db_env!=nullptr ? cache_db_env : db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (" << mdb_strerror(rc) << ") sfs " << db_path;
		clear_mmap_read_error(tid);
		return -1;
	}

	int64_t ret;

	if (!find_freemap_ext(txn, tid, ret, len))
	{
		ret = -1;
	}

	mdb_txn_abort(txn);

	clear_mmap_read_error(tid);

	return ret;
}

int64_t SingleFileStorage::get_free_space_slow(bool verbose, int64_t& freespace_extents, std::vector<SingleFileStorage::SPunchItem>* items)
{
	freespace_extents = 0;

	MDB_txn* txn;
	int rc = mdb_txn_begin(cache_db_env!=nullptr ? cache_db_env : db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return -1;
	}

	rc = mdb_set_compare(txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error setting compare func (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return -1;
	}

	int64_t ret = 0;

	int64_t prev_offset = -1;
	int64_t prev_len;

	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap get_free_space_slow (" << mdb_strerror(rc) << ") sfs " << db_path;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen))
			{
				assert(dkey.getStreampos() == dkey.getSize());
				assert(dval.getStreampos() == dval.getSize());

				ret += clen;

				assert(prev_offset==-1 || prev_offset + prev_len < cstart);

				prev_offset = cstart;
				prev_len = clen;

				if (verbose)
				{
					XLOG(INFO) << "Free extent at " << std::to_string(cstart) << " len " << std::to_string(clen);
				}

				if (items != nullptr)
				{
					items->push_back(SPunchItem(cstart, clen));
				}
			}
			else
			{
				assert(false);
			}

			++freespace_extents;
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	mdb_txn_abort(txn);

	int64_t curr_ext_add = 0;
	{
		std::scoped_lock lock(datafileoffset_mutex);
		if (data_file_offset_end > 0
			&& data_file_offset_end <= div_up(data_file.size(), block_size)*block_size)
		{
			curr_ext_add = data_file_offset_end - data_file_offset;

			if (verbose)
			{
				XLOG(INFO) << "Curr ext add " << std::to_string(curr_ext_add) << " data_file_offset " << std::to_string(data_file_offset) << " data_file_offset_end " << std::to_string(data_file_offset_end);
			}
		}
	}

	return ret + curr_ext_add;
}

bool SingleFileStorage::check_len_idx()
{
	MDB_txn* txn;
	int rc = mdb_txn_begin(cache_db_env!=nullptr ? cache_db_env : db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}

	rc = mdb_set_compare(txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error setting compare func (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return false;
	}

	rc = mdb_set_compare(txn, dbi_free_len, mdb_cmp_varint_rev);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error setting compare func (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(txn);
		return false;
	}

	MDB_cursor* it_cursor;
	mdb_cursor_open(txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	bool ret = true;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen)
				&& dkey.getStreampos() == dkey.getSize()
				&& dval.getStreampos() == dval.getSize())
			{
				rc = mdb_get(txn, dbi_free_len, &tval, &tkey);

				if (rc)
				{
					XLOG(ERR) << "Could not find free len ext (" << std::to_string(cstart) << ", "<< std::to_string(clen) << ") (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
			}
			else
			{
				ret = false;
				break;
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	if (!ret)
	{
		mdb_txn_abort(txn);
		return ret;
	}

	mdb_cursor_open(txn, dbi_free_len, &it_cursor);
	op = MDB_FIRST;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (" << mdb_strerror(rc) << ") sfs " << db_path;
			ret = false;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&clen)
				&& dval.getVarInt(&cstart)
				&& dkey.getStreampos() == dkey.getSize()
				&& dval.getStreampos() == dval.getSize())
			{
				rc = mdb_get(txn, dbi_free, &tval, &tkey);

				if (rc)
				{
					XLOG(ERR) << "Could not find free ext (" << std::to_string(cstart) << ", " << std::to_string(clen) << ") (" << mdb_strerror(rc) << ") sfs " << db_path;
					ret = false;
					break;
				}
			}
			else
			{
				ret = false;
				break;
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	mdb_txn_abort(txn);

	return ret;
}

void SingleFileStorage::defrag(str_map& params, relaxed_atomic<int64_t>& defrag_items)
{
	{
		std::scoped_lock lock(mutex);
		if (is_defragging)
		{
			XLOG(WARN) << "Already defragging. Not starting another defrag";
			return;
		}

		if (!allow_defrag)
		{
			XLOG(WARN) << "Defrag is not allowed (1)";
			return;
		}
	}

	if (params.find("ratio") == params.end()
		&& params.find("chunksize")==params.end() )
	{
		XLOG(ERR) << "Defrag setting 'ratio' or 'chunksize' not specified for defrag";
		return;
	}

	float dratio = static_cast<float>(atof(params["ratio"].c_str()));
	int64_t window = std::atoll(params["window"].c_str());
	int64_t chunksize = std::atoll(params["chunksize"].c_str());

	size_t n_chunks = 1;
	if (params.find("n_chunks") != params.end())
	{
		n_chunks = std::atoi(params["n_chunks"].c_str());
	}
	
	XLOG(INFO) << "Defrag settings: dratio=" << std::to_string(dratio) << " window=" << folly::prettyPrint(window, folly::PRETTY_BYTES_IEC) << " chunksize=" << folly::prettyPrint(chunksize, folly::PRETTY_BYTES_IEC) << " n_chunks=" << std::to_string(n_chunks);

	stop_defrag = false;

	THREAD_ID tid = gettid();
	setup_mmap_read_error(tid);

	MDB_txn* freespace_txn;
	if(cache_db_env!=nullptr)
	{
		int rc = mdb_txn_begin(cache_db_env, NULL, MDB_RDONLY, &freespace_txn);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Error starting freepsace transaction for read (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
			return;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB:  Error starting freespace transaction for read (2) (SIGBUS) sfs " << db_path;
			return;
		}
	}
	else
	{
		int rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &freespace_txn);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Error starting transaction for read (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
			return;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB:  Error starting transaction for read (2) (SIGBUS) sfs " << db_path;
			return;
		}
	}


	int rc = mdb_set_compare(freespace_txn, dbi_free, mdb_cmp_varint);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error setting compare func (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
		mdb_txn_abort(freespace_txn);
		return;
	}

	XLOG(INFO) << "Starting defrag of " << data_file_path << " dratio=" << std::to_string(dratio);

	int64_t prev_offset = -1;
	int64_t prev_len;

	std::vector<SPunchItem> defrag_extents;

	struct SDefragInfo
	{
		SDefragInfo()
			: freespace(0), max_cont_freespace(0), frag(-1) {}

		int64_t freespace;
		int64_t max_cont_freespace;
		double frag;
		size_t idx;
	};

	std::vector<SDefragInfo> chunks_defraginfo;

	MDB_cursor* it_cursor;
	mdb_cursor_open(freespace_txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;

		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap defrag (" << mdb_strerror(rc) << ") sfs " << db_path;
			break;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB:  Error getting item in freemap defrag (SIGBUS) sfs " << db_path;
			break;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen))
			{
				assert(dkey.getStreampos() == dkey.getSize());
				assert(dval.getStreampos() == dval.getSize());

				assert(prev_offset == -1 || prev_offset + prev_len < cstart);

				if (chunksize == 0)
				{
					if (prev_offset != -1)
					{
						int64_t used_len = cstart - (prev_offset + prev_len);
						int64_t potential_continuous_extent = prev_len + used_len + clen;
						int64_t curr_continuous_extent = (std::max)(prev_len, clen);
						int64_t more_continuous_extent = potential_continuous_extent - curr_continuous_extent;

						if (static_cast<float>(used_len) < more_continuous_extent * dratio)
						{
							XLOG(INFO) << "Can create continous extent of " << folly::prettyPrint(potential_continuous_extent, folly::PRETTY_BYTES_IEC) << " (" + folly::prettyPrint(more_continuous_extent, folly::PRETTY_BYTES_IEC) << " more)"
								<< " from (" << folly::prettyPrint(prev_len, folly::PRETTY_BYTES_IEC) << ", " << folly::prettyPrint(clen, folly::PRETTY_BYTES_IEC) << ") by defragging " << folly::prettyPrint(used_len + 2 * window, folly::PRETTY_BYTES_IEC) << " at " << std::to_string(prev_offset + prev_len - window)
								<< " (offset " << std::to_string(prev_offset + prev_len) << " len " << std::to_string(used_len) << ")";
							defrag_extents.push_back(SPunchItem(prev_offset + prev_len - window, used_len + window));

							if (defrag_extents.size() > max_defrag_extents)
							{
								XLOG(INFO) << "Max number of defrag extents reached (" << std::to_string(max_defrag_extents) << ")";
								break;
							}
						}
					}
				}
				else
				{
					size_t chunkidx = cstart / chunksize;

					if (chunkidx >= chunks_defraginfo.size())
					{
						chunks_defraginfo.resize(chunkidx + 1);
					}

					chunks_defraginfo[chunkidx].freespace += clen;
					if (clen > chunks_defraginfo[chunkidx].max_cont_freespace)
						chunks_defraginfo[chunkidx].max_cont_freespace = clen;
				}

				prev_offset = cstart;
				prev_len = clen;
			}
			else
			{
				assert(false);
			}
		}

		if (stop_defrag)
		{
			mdb_cursor_close(it_cursor);
			mdb_txn_abort(freespace_txn);
			XLOG(WARN) << "Defrag stopped (1)";
			return;
		}

	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	mdb_txn_abort(freespace_txn);

	{
		std::scoped_lock lock(mutex);

		if (!allow_defrag)
		{
			XLOG(WARN) << "Defrag is not allowed (2)";
			return;
		}

		is_defragging = true;

		add_defrag_skip_items_queue();
	}

	if (chunksize != 0)
	{
		for (size_t i = 0; i < chunks_defraginfo.size(); ++i)
		{
			chunks_defraginfo[i].idx = i;
			if (chunks_defraginfo[i].freespace <= 0)
				continue;

			double frag = 1.0 - (static_cast<double>(chunks_defraginfo[i].max_cont_freespace) / chunks_defraginfo[i].freespace);
			chunks_defraginfo[i].frag = frag;

			XLOG(INFO) << "Chunk " << std::to_string(i) << " at " << folly::prettyPrint(i * chunksize, folly::PRETTY_BYTES_IEC) << " len " << folly::prettyPrint(chunksize, folly::PRETTY_BYTES_IEC) << " "
				"Frag ratio=" << std::to_string(frag * 100) << "% free space " << folly::prettyPrint(chunks_defraginfo[i].freespace, folly::PRETTY_BYTES_IEC)
				<< " max continuous free space " << folly::prettyPrint(chunks_defraginfo[i].max_cont_freespace, folly::PRETTY_BYTES_IEC);
		}


		auto heap_comp = [](const SDefragInfo& a, const SDefragInfo& b) {
			return a.frag < b.frag;
		};

		std::make_heap(chunks_defraginfo.begin(), chunks_defraginfo.end(), heap_comp);

		for (size_t i = 0; i < n_chunks && !chunks_defraginfo.empty(); ++i)
		{
			std::pop_heap(chunks_defraginfo.begin(), chunks_defraginfo.end(), heap_comp);
			const SDefragInfo& curr = chunks_defraginfo.back();

			XLOG(INFO) << "Selected chunk " << std::to_string(curr.idx) << " at " << folly::prettyPrint(curr.idx* chunksize, folly::PRETTY_BYTES_IEC) << " len " << folly::prettyPrint(chunksize, folly::PRETTY_BYTES_IEC) << " for defrag. "
				"Frag ratio=" << std::to_string(curr.frag * 100) << "% free space " << folly::prettyPrint(curr.freespace, folly::PRETTY_BYTES_IEC)
				<< " max continuous free space " << folly::prettyPrint(curr.max_cont_freespace, folly::PRETTY_BYTES_IEC);

			SPunchItem defrag_ext(curr.idx * chunksize, chunksize);
			defrag_extents.push_back(defrag_ext);

			std::scoped_lock lock(mutex);
			curr_free_skip_extents.push_back(defrag_ext);

			chunks_defraginfo.pop_back();
		}

		if(defrag_extents.empty())
		{
			XLOG(WARN) << "Did not find chunk to defragment";
			std::scoped_lock lock(mutex);
			is_defragging = false;
			defrag_skip_items.clear();
			curr_free_skip_extents.clear();
			return;
		}

		std::sort(defrag_extents.begin(), defrag_extents.end());
	}
	
	MDB_txn* txn;
	rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &txn);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (3) (" << mdb_strerror(rc) << ") sfs " << db_path;
		std::scoped_lock lock(mutex);
		is_defragging = false;
		defrag_skip_items.clear();
		curr_free_skip_extents.clear();
		return;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Error starting transaction for read (3) (SIGBUS) sfs " << db_path;
		std::scoped_lock lock(mutex);
		is_defragging = false;
		defrag_skip_items.clear();
		curr_free_skip_extents.clear();
		return;
	}

	if(key_compare_func)
	{
		rc = mdb_set_compare(txn, dbi_main, key_compare_func);
		if (rc)
		{
			XLOG(ERR) << "LMDB: Error setting main compare func (2) (" << mdb_strerror(rc) << ") sfs " << db_path;
			std::scoped_lock lock(mutex);
			is_defragging = false;
			defrag_skip_items.clear();
			curr_free_skip_extents.clear();
			return;
		}
	}

	XLOG(INFO) << "Defragging using txn id " << std::to_string(mdb_get_txnid(txn));

	size_t defrag_write_errors = 0;
	bool do_restart = true;
	std::string continue_key;
	auto lasttime = std::chrono::steady_clock::now();
	while (do_restart)
	{
		do_restart = false;

		mdb_cursor_open(txn, dbi_main, &it_cursor);
		op = MDB_FIRST;

		MDB_val tkey;

		if (!continue_key.empty())
		{
			op = MDB_SET_RANGE;
			tkey.mv_data = &continue_key[0];
			tkey.mv_size = continue_key.size();
		}

		do
		{
			if (stop_defrag)
			{
				mdb_txn_abort(txn);

				{
					std::scoped_lock lock(mutex);
					is_defragging = false;
					defrag_skip_items.clear();
					curr_free_skip_extents.clear();
				}

				XLOG(WARN) << "Defrag stopped (2)";
				return;
			}

			MDB_val tval;
			rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
			op = MDB_NEXT;

			if (rc && rc != MDB_NOTFOUND)
			{
				XLOG(ERR) << "LMDB: Error getting item in main defrag (" << mdb_strerror(rc) << ") sfs " << db_path;
				break;
			}
			if (has_mmap_read_error_reset(tid))
			{
				XLOG(ERR) << "LMDB: Error getting item in main defrag (SIGBUS) sfs " << db_path;
				break;
			}

			if (rc != MDB_NOTFOUND)
			{
				std::string curr_key = std::string(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);

				if (curr_key == continue_key)
					continue;

				CRData rdata(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

				SPunchItem first_ext;

				if (!rdata.getVarInt(&first_ext.offset)
					|| !rdata.getVarInt(&first_ext.len))
				{
					continue;
				}

				std::vector<SPunchItem> extents;

				if(!read_extra_exts(first_ext.offset, rdata, extents))
				{
					XLOG(ERR) << "Error reading extra exts for defrag";
					continue;
				}

				for (size_t i = 0; i < extents.size() + 1; ++i)
				{
					SPunchItem& ext = (i == 0) ? first_ext : extents[i - 1];
					bool ext_found = false;
					auto it = std::upper_bound(defrag_extents.begin(), defrag_extents.end(), ext);
					if (it != defrag_extents.begin())
					{
						--it;
						ext_found = true;
					}

					if (ext_found
						&& it->offset <= ext.offset
						&& it->offset + it->len >= ext.offset + ext.len)
					{
						continue_key = curr_key;

						std::string fn = decompress_filename(continue_key);

						int64_t last_modified;
						std::string md5sum;

						if (!rdata.getVarInt(&last_modified)
							|| !rdata.getStr2(&md5sum))
						{
							break;
						}

						lock_defrag(continue_key);

						if (is_defrag_skip_item(continue_key))
						{
							unlock_defrag(continue_key);
							break;
						}

						int64_t fn_size = first_ext.len;

						for (SPunchItem& eext : extents)
							fn_size += eext.len;

						XLOG(INFO) << "Rewriting item " << fn << " size " << folly::prettyPrint(fn_size, folly::PRETTY_BYTES_IEC) << " because ext " << std::to_string(ext.offset) << " len " << std::to_string(ext.len) << " (defrag) sfs " << db_path << " defrag ext (" << std::to_string(it->offset) << ", " << std::to_string(it->len) << ")";
						++defrag_items;

						/*char* data;
						size_t data_len;
						if (read(first_ext.offset, first_ext.len, extents, fn, 0, data, data_len))
						{
							int rc = write_int(fn, data, data_len, last_modified, md5sum, false, false, std::string::npos);
							if(rc!=0)
							{
								if (rc == ENOSPC)
								{
									XLOG(WARN) << "Error defragging item " << fn << ". Out of space. rc " << std::to_string(rc);
									stop_defrag = true;
								}
								else if (rc!=EDEADLK)
								{
									XLOG(WARN) << "Error defragging item " << fn << " rc " << std::to_string(rc);

									++defrag_write_errors;
									if (defrag_write_errors > 10)
										stop_defrag = true;
								}
							}
							else
							{
								defrag_write_errors = 0;
							}
							free(data);
						}*/

						unlock_defrag(continue_key);

						break;
					}
				}

				auto ctime = std::chrono::steady_clock::now();
				std::unique_lock lock(mutex);
				if (ctime - lasttime > 5min
					|| defrag_restart==1)
				{
					mdb_txn_abort(txn);

					defrag_skip_items.clear();

					if (defrag_restart != 1)
					{
						add_defrag_skip_items_queue();

						lock.unlock();
					}
					else
					{
						XLOG(INFO) << "Restarting defrag after commit. Waiting for commit...";
						defrag_restart = 2;

						while (defrag_restart == 2)
						{
							lock.unlock();
							std::this_thread::sleep_for(100ms);
							lock.lock();
						}

						add_defrag_skip_items_queue();
						
						lock.unlock();
						XLOG(INFO) << "Commit finished. Restarting transaction and continuing defrag...";
					}

					XLOG(INFO) << "Restarting defrag transaction";
					
					rc = mdb_txn_begin(db_env, NULL, MDB_RDONLY, &txn);
					if (rc)
					{
						XLOG(ERR) << "LMDB: Error starting transaction for read (4) (" << mdb_strerror(rc) << ") sfs " << db_path;
						break;
					}
					if (has_mmap_read_error_reset(tid))
					{
						XLOG(ERR) << "LMDB: Error starting transaction for read (4) (SIGBUS) sfs " << db_path;
						break;
					}

					if(key_compare_func)
					{
						rc = mdb_set_compare(txn, dbi_main, key_compare_func);
						if (rc)
						{
							XLOG(ERR) << "LMDB: Error setting main compare func (3) (" << mdb_strerror(rc) << ") sfs " << db_path;
							break;
						}
					}

					XLOG(INFO) << "Defragging using txn id " + std::to_string(mdb_get_txnid(txn));

					do_restart = true;
					lasttime = std::chrono::steady_clock::now();
				}
			}
		} while (!do_restart
					&& rc != MDB_NOTFOUND);
	}

	mdb_txn_abort(txn);

	{
		std::scoped_lock lock(mutex);
		is_defragging = false;
		defrag_skip_items.clear();
		curr_free_skip_extents.clear();
	}
}

bool SingleFileStorage::start_thread(int64_t transid)
{
	curr_transid = transid;

	commit_thread_h = std::thread([this](){
		(*this)();
	});

	return true;
}

bool SingleFileStorage::regen_datafile_free(MDB_txn* freespace_txn)
{
	THREAD_ID tid = gettid();
	bool ret=true;
	MDB_cursor* it_cursor;
	mdb_cursor_open(freespace_txn, dbi_free, &it_cursor);
	MDB_cursor_op op = MDB_FIRST;
	int rc;
	do
	{
		MDB_val tkey;
		MDB_val tval;
		rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);
		op = MDB_NEXT;
		if (rc && rc != MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (" << mdb_strerror(rc) << ")";
			ret=false;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Error getting item in freemap startup (SIGBUS)";
			ret=false;
		}

		if (rc != MDB_NOTFOUND)
		{
			CRData dkey(reinterpret_cast<char*>(tkey.mv_data), tkey.mv_size);
			CRData dval(reinterpret_cast<char*>(tval.mv_data), tval.mv_size);

			int64_t cstart;
			int64_t clen;
			if (dkey.getVarInt(&cstart)
				&& dval.getVarInt(&clen)
				&& dkey.getStreampos() == dkey.getSize()
				&& dval.getStreampos() == dval.getSize())
			{
				data_file_free += clen;
			}
			else
			{
				ret=false;
				XLOG(WARN) << "Deleting invalid freemap extent";
				mdb_cursor_del(it_cursor, 0);
				if (has_mmap_read_error_reset(tid))
				{
					XLOG(ERR) << "LMDB: Error deleting freemap item in startup (SIGBUS)";
				}
			}
		}
	} while (rc != MDB_NOTFOUND);

	mdb_cursor_close(it_cursor);

	return ret;
}

bool SingleFileStorage::regen_free_len_idx(MDB_txn* freespace_txn)
{
	THREAD_ID tid = gettid();
	if (data_file_free > 0)
	{
		MDB_cursor* it_cursor;
		mdb_cursor_open(freespace_txn, dbi_free_len, &it_cursor);
		MDB_cursor_op op = MDB_FIRST;
		MDB_val tkey;
		MDB_val tval;
		int rc = mdb_cursor_get(it_cursor, &tkey, &tval, op);

		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Freelen idx (SIGBUS)";
			return false;
		}

		if (rc == MDB_NOTFOUND)
		{
			XLOG(INFO) << "Free len idx empty. Generating it...";

			if (!generate_free_len_idx(freespace_txn))
			{
				XLOG(ERR) << "Error generating free_len idx";
				return false;
			}
		}
	}
	return true;
}

bool SingleFileStorage::read_pgids(MDB_txn* txn, MDB_dbi dbi, THREAD_ID tid,
	TmpMmapedPgIds& mmap_pg_ids)
{
	MDB_cursor* cur;
	int rc = mdb_cursor_open(txn, dbi, &cur);
	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed to open cursor for read_pgids (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed to open cursor for read_pgids (SIGBUS) sfs " << db_path;
		is_dead = true;
		do_stop_on_error();
		return false;
	}

	size_t pgno;

	rc = mdb_cursor_first_leaf_page(cur, &pgno);

	if (rc)
	{
		XLOG(ERR) << "LMDB: Failed mdb_cursor_first_leaf_page in read_pgids (" << mdb_strerror(rc) << ") sfs " << db_path;
		return false;
	}
	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "LMDB: Failed mdb_cursor_first_leaf_page for read_pgids (SIGBUS) sfs " << db_path;
		is_dead = true;
		do_stop_on_error();
		return false;
	}

	mmap_pg_ids.add_pgid(pgno);

	while (rc == MDB_SUCCESS)
	{
		rc = mdb_cursor_next_leaf_page(cur, &pgno);

		if (rc && rc!=MDB_NOTFOUND)
		{
			XLOG(ERR) << "LMDB: Failed mdb_cursor_next_leaf_page in read_pgids (" << mdb_strerror(rc) << ") sfs " << db_path;
			return false;
		}
		if (has_mmap_read_error_reset(tid))
		{
			XLOG(ERR) << "LMDB: Failed mdb_cursor_next_leaf_page for read_pgids (SIGBUS) sfs " << db_path;
			is_dead = true;
			do_stop_on_error();
			return false;
		}

		if(rc!=MDB_NOTFOUND)
			mmap_pg_ids.add_pgid(pgno);
	}

	std::sort(mmap_pg_ids.begin(), mmap_pg_ids.end());

	return true;
}

void SingleFileStorage::do_stop_on_error()
{
	if(is_dead || write_offline)
	{
		XLOG(ERR) << "Stopping on error rc=1.";
		_exit(1);
	}
}

SingleFileStorage::TmpMmapedPgIds::TmpMmapedPgIds()
	: mmap_ptr(nullptr), n_pgids(0)
{
	std::string tmp_fn = "/var/tmp";
	backing_file = File(tmp_fn, O_RDWR | O_CLOEXEC | O_TMPFILE );

	mmap_size = 10ULL * 1024 * 1024 * 1024;
	ftruncate64(backing_file.fd(), mmap_size);

	mmap_ptr = reinterpret_cast<char*>(mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
		MAP_SHARED, backing_file.fd(), 0));

	if (mmap_ptr == reinterpret_cast<char*>(-1))
	{
		XLOG(ERR) << "Error creating mmap of mmap-pgids. " << folly::errnoStr(errno);
		abort();
	}
}

SingleFileStorage::TmpMmapedPgIds::~TmpMmapedPgIds()
{
	if (mmap_ptr != nullptr)
	{
		munmap(mmap_ptr, mmap_size);
	}
}

std::pair<int64_t, std::string> SingleFileStorage::get_next_partid()
{
	int64_t local_curr_partid;
	{
    	std::scoped_lock lock(reserved_extents_mutex);
    	++curr_partid;
		local_curr_partid = curr_partid;
	}
    
	return std::make_pair(local_curr_partid,
		encrypt_id(local_curr_partid));
}

int64_t SingleFileStorage::get_next_version()
{
	std::scoped_lock lock(reserved_extents_mutex);
	++curr_version;
	return curr_version;
}

int64_t SingleFileStorage::decrypt_id(const std::string& encdata)
{
	return decryptId(encdata, enckey);
}

std::string SingleFileStorage::encrypt_id(const int64_t id)
{
	return cryptId(id, enckey);
}

namespace
{
	
int64_t currentNanoseconds()
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

} // anon namespace

int SingleFileStorage::add_new_object(MDB_txn* txn, const THREAD_ID tid, const std::string& fn)
{
	CWData key_data;
	key_data.addVarInt(currentNanoseconds());

	MDB_val tkey = {.mv_size=key_data.getDataSize(), .mv_data=key_data.getDataPtr()};

	CWData data;
	data.addString2(fn);
	data.addString2(std::string());

	MDB_val tval = {.mv_size=data.getDataSize(), .mv_data=data.getDataPtr()};

	const auto rc = mdb_put(txn, dbi_new_objects, &tkey, &tval, 0);

	if (rc)
	{
		XLOG(ERR) << "Error adding new object to db (" << mdb_strerror(rc) << ") sfs " << db_path;
		return 1;
	}

	if (has_mmap_read_error_reset(tid))
	{
		XLOG(ERR) << "Error adding new object to db (SIGBUS) sfs " << db_path;
		return 1;
	}

	return 0;
}