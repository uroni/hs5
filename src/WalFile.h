#include <string>
#include "File.h"
#include "SingleFileStorage.h"
#include <mutex>
#include "data.h"
#include <vector>
#include <condition_variable>
#include <optional>
#include <map>

class WalFile
{
    File primaryFile;
    size_t primaryFilePendingData = 0;
    File altFile;
    size_t altFilePendingData = 0;

    bool pendingReset = false;

    std::condition_variable pendingDataCond;

    MultiFile& dataFile;

    bool useAltFile = false;

    File& currentFile()
    {
        return useAltFile ? altFile : primaryFile;
    }

    File& currentAltFile()
    {
        return useAltFile ? primaryFile : altFile;
    }

    File& currentFileManual(bool alt)
    {
        return alt ? altFile : primaryFile;
    }
    
    void incrPendingData()
    {
        if(useAltFile)
            ++altFilePendingData;
        else
            ++primaryFilePendingData;
    }

    bool decrPendingData(const bool alt)
    {
        if(alt)
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

    size_t& altPendingData()
    {
        return useAltFile ? primaryFilePendingData : altFilePendingData;
    }

    int64_t offset = 0;
    size_t _items = 0;
    mutable std::mutex mutex;
    int64_t seqNo = 0;
    std::string walUuid;

    bool needsWrite(const SingleFileStorage::SFragInfo& info);

    bool writeData(CWData& data, bool* usedAlt = nullptr);

    struct DataItem
    {
        int64_t dataOff;
        bool isAlt;
        std::vector<char> data;
    };

    std::condition_variable dataWriteCond;
    std::queue<DataItem> dataWriteQueue;

    struct DataItemRef
    {
        size_t refs;
    };

    std::map<int64_t, DataItemRef> dataItems;

    std::condition_variable writeDoneCond;

public:
    WalFile(const std::string &path, const std::string_view walUuid, MultiFile& dataFile);

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
        std::optional<CommitInfo> commit_info;
    };

    ReadResult read(int64_t transid, MultiFile* data_file, const bool initOnly, const bool readFromAltFile);

    struct ResetPrep
    {
        WalFile* walFile;
        std::unique_lock<std::mutex> lock;

        ResetPrep(WalFile* walFile)
            : walFile(walFile)
        {
            if(walFile)
            {
                std::unique_lock<std::mutex> nl(walFile->mutex);
                lock.swap(nl);
            }
        }

        void unlock()
        {
            if(walFile)
                lock.unlock();
        }

        ResetPrep(const ResetPrep&) = delete;
        ResetPrep& operator=(const ResetPrep&) = delete;
    };

    
    void reset(ResetPrep& prep, const bool sync, const std::optional<bool> useAltFileManual);
    bool sync(const CommitInfo& commitInfo);
    bool fsync();
    bool switchFiles();

    size_t items() const
    {
        std::scoped_lock lock(mutex);
        return _items;
    }

    int64_t size() const
    {
        std::scoped_lock lock(mutex);
        return offset;
    }

    void waitForWrites();

    void waitForWriteout(const int64_t off, const size_t dataSize);

    void wakeupDataWriteThread()
    {
        std::scoped_lock lock(mutex);
        dataWriteCond.notify_one();
    }

    void dataWriteThread(std::stop_token stopToken);
};