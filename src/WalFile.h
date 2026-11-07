#include "File.h"
#include "SingleFileStorage.h"
#include "ThreadSafety.h"
#include "data.h"
#include <atomic>
#include <cassert>
#include <condition_variable>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

class WalFile
{
    mutable std::mutex mutex;

    File primaryFile;
    size_t primaryFilePendingData GUARDED_BY(mutex) = 0;
    std::set<int64_t> primaryFileDirtyBlocks GUARDED_BY(mutex);
    File altFile;
    size_t altFilePendingData GUARDED_BY(mutex) = 0;
    std::set<int64_t> altFileDirtyBlocks GUARDED_BY(mutex);

    bool pendingReset GUARDED_BY(mutex) = false;

    std::condition_variable pendingDataCond;

    MultiFile& dataFile;

    bool useAltFile GUARDED_BY(mutex) = false;

    File& currentFile() REQUIRES(mutex)
    {
        return useAltFile ? altFile : primaryFile;
    }

    File& currentAltFile() REQUIRES(mutex)
    {
        return useAltFile ? primaryFile : altFile;
    }

    std::set<int64_t>& dirtyBlocks() REQUIRES(mutex)
    {
        return useAltFile ? altFileDirtyBlocks : primaryFileDirtyBlocks;
    }

    std::set<int64_t>& altDirtyBlocks() REQUIRES(mutex)
    {
        return useAltFile ? primaryFileDirtyBlocks : altFileDirtyBlocks;
    }

    const std::set<int64_t>& dirtyBlocks() const REQUIRES(mutex)
    {
        return useAltFile ? altFileDirtyBlocks : primaryFileDirtyBlocks;
    }

    const std::set<int64_t>& altDirtyBlocks() const REQUIRES(mutex)
    {
        return useAltFile ? primaryFileDirtyBlocks : altFileDirtyBlocks;
    }

    File& currentFileManual(bool alt) REQUIRES(mutex)
    {
        return alt ? altFile : primaryFile;
    }

    std::set<int64_t>& dirtyBlocksManual(bool alt) REQUIRES(mutex)
    {
        return alt ? primaryFileDirtyBlocks : altFileDirtyBlocks;
    }

    void incrPendingData() REQUIRES(mutex)
    {
        if (useAltFile)
            ++altFilePendingData;
        else
            ++primaryFilePendingData;
    }

    bool lastPendingData(const bool alt) REQUIRES(mutex)
    {
        if (alt)
            return altFilePendingData == 1;
        else
            return primaryFilePendingData == 1;
    }

    bool decrPendingData(const bool alt) REQUIRES(mutex)
    {
        if (alt)
        {
            --altFilePendingData;
            return altFilePendingData == 0;
        }
        else
        {
            --primaryFilePendingData;
            return primaryFilePendingData == 0;
        }
    }

    size_t& altPendingData() REQUIRES(mutex)
    {
        return useAltFile ? primaryFilePendingData : altFilePendingData;
    }

    int64_t offset GUARDED_BY(mutex) = 0;
    size_t _items GUARDED_BY(mutex) = 0;
    int64_t seqNo GUARDED_BY(mutex) = 0;

    // Pre-fills message headers without holding mutex. writeData() rewrites
    // the header (with mutex held) if seqNo changed in the meantime
    int64_t unlockedSeqNo() const NO_THREAD_SAFETY_ANALYSIS
    {
        return seqNo;
    }
    std::string walUuid;

    bool needsWrite(const SingleFileStorage::SFragInfo& info);

    struct DataItem
    {
        int64_t dataOff;
        bool isAlt;
        std::vector<char> data;
    };

    bool writeData(CWData& data, DataItem* dataItem = nullptr) EXCLUDES(mutex);

    template <typename F>
    bool forEachDataItem(const int64_t off, const size_t dataSize, F fn) const REQUIRES(mutex);

    std::condition_variable dataWriteCond;
    std::queue<DataItem> dataWriteQueue GUARDED_BY(mutex);

    struct DataItemRef
    {
        size_t refs;
    };

    std::map<int64_t, DataItemRef> dataItems GUARDED_BY(mutex);

    std::condition_variable writeDoneCond;

    bool trackDirtyBlocks;
    // Skip fsync (for testing only)
    bool noSync;

    template <typename F>
    int fsyncFile(F& file)
    {
        if (noSync)
            return 0;
        return file.fsyncNoInt();
    }

#ifndef NDEBUG
    // Thread of the first assertCommitThread() call
    std::atomic<std::thread::id> commitThreadId;
#endif

    // Asserts that all callers are the same (commit) thread
    void assertCommitThread()
    {
#ifndef NDEBUG
        const auto self = std::this_thread::get_id();
        std::thread::id expected;
        if (!commitThreadId.compare_exchange_strong(expected, self))
            assert(expected == self);
#endif
    }

    // File currently written to, without holding mutex. Only for the thread
    // calling switchFiles() (the commit thread), which is the only writer of useAltFile
    File& unlockedCurrentFile() NO_THREAD_SAFETY_ANALYSIS
    {
        assertCommitThread();
        return currentFile();
    }

    void resetLocked(const bool sync, const std::optional<bool> useAltFileManual) REQUIRES(mutex);

    // Tells thread safety analysis that lock (std::unique_lock is not annotated) holds mutex
    void assertHeld(const std::unique_lock<std::mutex>& lock) const ASSERT_CAPABILITY(mutex)
    {
        assert(lock.owns_lock() && lock.mutex() == &mutex);
    }

public:
    WalFile(const std::string& path, const std::string_view walUuid, MultiFile& dataFile, const bool trackDirtyBlocks, const bool noSync);

    bool write(const int64_t transid, const SingleFileStorage::SFragInfo& info);
    bool writeData(const int64_t off, const char* data, const size_t dataSize, const bool useThreadWrite);
    bool writeAddFreemapExt(const int64_t transid, const int64_t off, const int64_t len);
    bool writeDelFreemapExt(const int64_t transid, const int64_t off, const int64_t len);

    struct CommitInfo
    {
        int64_t data_file_max_size;
        int64_t curr_write_ext_start;
        int64_t curr_write_ext_end;
        int64_t data_file_free;
        int64_t curr_transid;
        int64_t local_curr_partid;
        int64_t local_curr_version;
        std::vector<SingleFileStorage::SPunchItem> reserved_extents;
    };

    struct ReadResult
    {
        std::vector<SingleFileStorage::SFragInfo> items;
        std::vector<std::unique_ptr<SingleFileStorage::SCommitInfo>> itemCommitInfos;
        std::vector<std::unique_ptr<SingleFileStorage::MatchInfo>> itemMatchInfos;
        std::optional<CommitInfo> commit_info;
    };

    ReadResult read(int64_t transid, MultiFile* data_file, const bool initOnly, const bool readFromAltFile) EXCLUDES(mutex);

    struct ResetPrep
    {
        WalFile* walFile;
        std::unique_lock<std::mutex> lock;

        ResetPrep(WalFile* walFile)
            : walFile(walFile)
        {
            if (walFile)
            {
                std::unique_lock<std::mutex> nl(walFile->mutex);
                lock.swap(nl);
            }
        }

        void unlock()
        {
            if (walFile)
                lock.unlock();
        }

        ResetPrep(const ResetPrep&) = delete;
        ResetPrep& operator=(const ResetPrep&) = delete;
    };

    void reset(ResetPrep& prep, const bool sync, const std::optional<bool> useAltFileManual);
    void reset(const bool sync, const std::optional<bool> useAltFileManual) EXCLUDES(mutex);
    bool sync(const CommitInfo& commitInfo);
    bool fsync();
    bool switchFiles() EXCLUDES(mutex);

    size_t items() const EXCLUDES(mutex)
    {
        std::scoped_lock lock(mutex);
        return _items;
    }

    int64_t size() const EXCLUDES(mutex)
    {
        std::scoped_lock lock(mutex);
        return offset;
    }

    void waitForWrites() EXCLUDES(mutex);

    void waitForWriteout(const int64_t off, const size_t dataSize) EXCLUDES(mutex);

    bool isDirty(const int64_t off, const size_t dataSize) const EXCLUDES(mutex);

    void wakeupDataWriteThread() EXCLUDES(mutex)
    {
        std::scoped_lock lock(mutex);
        dataWriteCond.notify_one();
    }

    void dataWriteThread(std::stop_token stopToken) EXCLUDES(mutex);
};