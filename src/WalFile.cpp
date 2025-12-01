/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "WalFile.h"
#include "data.h"
#include <folly/system/MemoryMapping.h>
#include <folly/hash/Checksum.h>
#include <folly/Random.h>
#include <folly/system/ThreadName.h>

const char walFileInitString[] = "WALFILE_INIT";
const char dataTypeFragInfo = 0;
const char dataTypeDataFileData = 1;
const char dataTypeInit = 2;
const char dataTypeSync = 3;

const int64_t waitBlockSize = 64 * 1024;


DEFINE_bool(wal_write_delay, false, "Delay WAL writes for testing");


namespace
{

void setChecksumAndSize(CWData& data)
{
    const auto dsize = static_cast<unsigned int>(data.getDataSize());
    memcpy(data.getDataPtr() + sizeof(dsize), &dsize, sizeof(dsize));
    const auto crc = folly::crc32c(reinterpret_cast<uint8_t*>(data.getDataPtr() + sizeof(dsize)), dsize - sizeof(dsize));    
    static_assert(sizeof(crc) == sizeof(unsigned int));
    memcpy(data.getDataPtr(), &crc, sizeof(crc));
}

void writeDataHeader(CWData& data, char type, int64_t seqNo)
{
    data.addUInt(0); // checksum
    data.addUInt(0); // size
    data.addChar(type); // version & type
    data.addVarInt(seqNo);
}

bool serializeCommitInfo(const WalFile::CommitInfo& commitInfo, CWData& data)
{
    data.addVarInt(commitInfo.data_file_max_size);
    data.addVarInt(commitInfo.curr_write_ext_start);
    data.addVarInt(commitInfo.curr_write_ext_end);
    data.addVarInt(commitInfo.data_file_free);
    data.addVarInt(commitInfo.curr_transid);
    data.addVarInt(commitInfo.local_curr_partid);
    data.addVarInt(commitInfo.local_curr_version);
    data.addVarInt(commitInfo.reserved_extents.size());
    for (const auto& ext : commitInfo.reserved_extents)
    {
        data.addVarInt(ext.offset);
        data.addVarInt(ext.len);
    }
    return true;
}

bool deserializeCommitInfo(CRData& data, WalFile::CommitInfo& commitInfo)
{
    bool b = true;
    b &= data.getVarInt(&commitInfo.data_file_max_size);
    b &= data.getVarInt(&commitInfo.curr_write_ext_start);
    b &= data.getVarInt(&commitInfo.curr_write_ext_end);
    b &= data.getVarInt(&commitInfo.data_file_free);
    b &= data.getVarInt(&commitInfo.curr_transid);
    b &= data.getVarInt(&commitInfo.local_curr_partid);
    b &= data.getVarInt(&commitInfo.local_curr_version);
    int64_t reserved_extents_size = 0;
    b &= data.getVarInt(&reserved_extents_size);
    commitInfo.reserved_extents.resize(reserved_extents_size);
    for (auto& ext : commitInfo.reserved_extents)
    {
        b &= data.getVarInt(&ext.offset);
        b &= data.getVarInt(&ext.len);
    }
    return b;
}

bool forEachDataItem(const int64_t off, const size_t dataSize, auto fn)
{
    auto startOff = (off / waitBlockSize) * waitBlockSize;
    for(auto i = startOff; i < off + dataSize; i += waitBlockSize)
    {
        const auto block = i / waitBlockSize;
        if(fn(block))
            return true;
    }
    return false;
}

} // namespace

WalFile::WalFile(const std::string &path, const std::string_view walUuid, MultiFile& dataFile)
    : primaryFile(path+"1", O_RDWR | O_CREAT | O_APPEND, 0644), 
    altFile(path+"2", O_RDWR | O_CREAT | O_APPEND, 0644),
    dataFile(dataFile),
    walUuid(walUuid)
{
    seqNo = 1;

    int64_t primaryFileSeqNo = 0;
    if(primaryFile.size() > 0 )
    {
        read(0, nullptr, true, false);
        primaryFileSeqNo = seqNo;
    }
    int64_t altFileSeqNo = 0;
    if(altFile.size() > 0 )
    {
        read(0, nullptr, true, true);
        altFileSeqNo = seqNo;
        if(altFileSeqNo > primaryFileSeqNo)
            useAltFile = true;
    }

    XLOGF(INFO, "WalFile initialized. Using {} file. primaryFileSeqNo={}, altFileSeqNo={}", 
        useAltFile ? "alt" : "primary", primaryFileSeqNo, altFileSeqNo);

    if(primaryFileSeqNo!=0 && primaryFileSeqNo == altFileSeqNo)
    {
        XLOGF(ERR, "WalFile: Both WAL files have same seqNo {}.", primaryFileSeqNo);
        abort();
    }

    if(currentFile().size() == 0)
    {
        WalFile::ResetPrep prep(nullptr);
        reset(prep, false, std::nullopt);
    }
}

bool WalFile::needsWrite(const SingleFileStorage::SFragInfo& info)
{
    switch(info.action)
    {
        case SingleFileStorage::FragAction::Add:
        case SingleFileStorage::FragAction::Del:
        case SingleFileStorage::FragAction::DelOld:
        case SingleFileStorage::FragAction::DelWithQueued:
        case SingleFileStorage::FragAction::QueueDel:
        case SingleFileStorage::FragAction::UnqueueDel:
        case SingleFileStorage::FragAction::RestoreOld:
        case SingleFileStorage::FragAction::FreeExtents:
        case SingleFileStorage::FragAction::DelFreemapWal:
        case SingleFileStorage::FragAction::AddFreemapWal:
        return true;
        default:
        return false;
    }
}

bool WalFile::write(const int64_t transid, const SingleFileStorage::SFragInfo& info)
{
    if(!needsWrite(info))
    {
        return true;
    }

    CWData data;
    writeDataHeader(data, dataTypeFragInfo, seqNo);
    data.addVarInt(transid);
    data.addUChar(static_cast<unsigned char>(info.action));
    data.addString2(info.fn);
    data.addVarInt(info.offset);
    data.addVarInt(info.len);
    data.addVarInt(info.last_modified);
    data.addString2(info.md5sum);
    data.addVarInt(info.extra_exts.size());
    for (const auto& ext : info.extra_exts)
    {
        data.addVarInt(ext.offset);
        data.addVarInt(ext.len);
    }

    return writeData(data);
}

bool WalFile::writeAddFreemapExt(const int64_t transid, const int64_t off, const int64_t len)
{
    SingleFileStorage::SFragInfo info;
    info.action = SingleFileStorage::FragAction::AddFreemapWal;
    info.offset = off;
    info.len = len;
    return write(transid, info);
}

bool WalFile::writeDelFreemapExt(const int64_t transid, const int64_t off, const int64_t len)
{
    SingleFileStorage::SFragInfo info;
    info.action = SingleFileStorage::FragAction::DelFreemapWal;
    info.offset = off;
    info.len = len;
    return write(transid, info);
}

bool WalFile::writeData(CWData& data, DataItem* dataItem)
{
    setChecksumAndSize(data);

    std::scoped_lock lock(mutex);

    const auto typeOff = 2*sizeof(unsigned int);
    const auto seqNoOff =  typeOff +  sizeof(char);
    CRData checkData(data.getDataPtr() + seqNoOff, data.getDataSize() - seqNoOff);
    int64_t msgSeqNo;
    if(!checkData.getVarInt(&msgSeqNo))
        abort();
    if(msgSeqNo != seqNo)
    {
        XLOGF(DBG, "WalFile: writeData: seqNo changed from {} to {} -- rewriting data", msgSeqNo, seqNo);
        const auto type = data.getDataPtr()[typeOff];
        CWData data2;
        writeDataHeader(data2, type, seqNo);
        data2.addBuffer(checkData.getCurrDataPtr(), checkData.getLeft());
        setChecksumAndSize(data2);

        if(currentFile().pwriteFull(data2.getDataPtr(), data2.getDataSize(), offset) != data2.getDataSize())
        {
            return false;
        }    
        offset += data2.getDataSize();
    }
    else
    {
        if(currentFile().pwriteFull(data.getDataPtr(), data.getDataSize(), offset) != data.getDataSize())
        {
            return false;
        }    
        offset += data.getDataSize();
    }

    ++_items;    

    if(dataItem == nullptr)
        return true;

    auto& item = *dataItem;

    item.isAlt = useAltFile;   

    const auto off = item.dataOff;;
    const auto dataSize = item.data.size();

    XLOGF(DBG0, "WalFile: queueing data {} write to data file at offset {} size {}", folly::crc32c(reinterpret_cast<const uint8_t*>(item.data.data()), item.data.size()), off, dataSize);

    dataWriteQueue.push(std::move(item));
    dataWriteCond.notify_one();

    assert(dataItem->data.empty());

    incrPendingData();

    forEachDataItem(off, dataSize, [&](const int64_t block)
    {
        auto it = dataItems.find(block);
        if(it != dataItems.end())
        {
            ++it->second.refs;
        }
        else
        {
            dataItems.insert({block, {.refs = 1}});
        }
        return false;
    });

    return true;
}

WalFile::ReadResult WalFile::read(int64_t transid, MultiFile* data_file, const bool initOnly, const bool readFromAltFile)
{
    std::scoped_lock lock(mutex);

    File& file = readFromAltFile ? currentAltFile() : currentFile();

    folly::MemoryMapping mapping(file.dupCloseOnExec());

    const auto allData = mapping.range();
    size_t off = 0;

    bool hasInit = false;

    WalFile::ReadResult ret;
    while(off + sizeof(unsigned int)*2 < allData.size())
    {
        unsigned int dsize;
        unsigned int crc;
        memcpy(&crc, allData.data() + off, sizeof(crc));
        memcpy(&dsize, allData.data() + off + sizeof(crc), sizeof(dsize));
        if(off + dsize > allData.size())
        {
            break;
        }
        if(crc != folly::crc32c(allData.data() + off + sizeof(crc), dsize - sizeof(crc)))
        {
            break;
        }

        CRData data(reinterpret_cast<const char*>(allData.data()) + off + sizeof(crc) + sizeof(dsize), dsize - sizeof(crc) - sizeof(dsize));

        off += dsize;

        char type;
        if(!data.getChar(&type))
            throw std::runtime_error("WalFile: read: failed to read type");

        int64_t msgSeqNo;

        if(!data.getVarInt(&msgSeqNo))
            throw std::runtime_error("WalFile: read: failed to read seqNo");

        if(!hasInit && type != dataTypeInit)
        {
            throw std::runtime_error("WalFile: read: missing WAL file header. Found type " + std::to_string(static_cast<int>(type)) + " instead. readFromAltFile=" + (readFromAltFile ? "true" : "false"));
        }        
        else if(type == dataTypeInit)
        {
            if(hasInit)
                throw std::runtime_error("WalFile: read: multiple WAL file headers found");

            hasInit = true;
            seqNo = msgSeqNo;

            std::string initStr;
            if(!data.getStr2(&initStr))
                throw std::runtime_error("WalFile: read: failed to read init string");

            if(initStr != walFileInitString)
                throw std::runtime_error("WalFile: read: invalid WAL file header magic string: " + initStr);

            std::string initWalUuid;
            if(!data.getStr2(&initWalUuid))
                throw std::runtime_error("WalFile: read: failed to read WAL UUID");
                
            if(initWalUuid != walUuid)
                throw std::runtime_error("WalFile: read: WAL UUID mismatch: " + folly::hexlify(walUuid) + " != " + folly::hexlify(initWalUuid));

            if(initOnly)
                return ret;

            continue;
        }

        if(seqNo != msgSeqNo)
        {
            XLOGF(WARN, "WalFile: read: seqNo mismatch, expected {}, got {} -- readFromAltFile={}", seqNo, msgSeqNo, readFromAltFile);
            // TODO: Remove abort. This might occurr for some file systems on power fail
            abort();
            break;
        }

        if(type == dataTypeFragInfo)
        {            
            int64_t ctransid;
            bool b = true;
            b &= data.getVarInt(&ctransid);            
            if(ctransid != transid)
            {            
                XLOGF(INFO, "WalFile: read: skipping entry for transid {}, looking for transid {}", ctransid, transid);
                continue;
            }
            SingleFileStorage::SFragInfo info;
            unsigned char action;
            b &= data.getUChar(&action);
            info.action = static_cast<SingleFileStorage::FragAction>(action);
            b &= data.getStr2(&info.fn);
            b &= data.getVarInt(&info.offset);
            b &= data.getVarInt(&info.len);
            b &= data.getVarInt(&info.last_modified);
            b &= data.getStr2(&info.md5sum);
            
            int64_t extra_exts_size = 0;
            b &= data.getVarInt(&extra_exts_size);
            
            for(size_t i = 0; i < extra_exts_size; ++i)
            {
                SingleFileStorage::SPunchItem ext;
                b &= data.getVarInt(&ext.offset);
                b &= data.getVarInt(&ext.len);
                info.extra_exts.push_back(ext);
            }

            if(!b)
            {
                throw std::runtime_error("WalFile: read: failed to read data");
            }

            XLOGF(INFO, "WalFile: recovery: read entry for transid {}, fn {}, action {}, offset {}, len {}", ctransid, info.fn, static_cast<int>(info.action), info.offset, info.len);

            ret.items.push_back(info);
        }
        else if(type==dataTypeDataFileData)
        {
            int64_t data_off;
            data.getVarInt(&data_off);

            const size_t data_size = data.getLeft();
            const char* data_ptr = data.getCurrDataPtr();

            XLOGF(INFO, "WalFile: recovery: writing data {} to data file at offset {} size {}", folly::crc32c(reinterpret_cast<const uint8_t*>(data_ptr), data_size), data_off, data_size);

            if(!data_file)
                throw std::runtime_error("WalFile: read: data file is null");

            if(data_file->pwriteFull(data_ptr, data_size, data_off) != data_size)
            {
                throw std::runtime_error("WalFile: read: failed to write data to data file: " + folly::errnoStr(errno));
            }
        }
        else if(type == dataTypeSync)
        {
            CommitInfo commitInfo;
            if(!deserializeCommitInfo(data, commitInfo))
                throw std::runtime_error("WalFile: read: failed to read commit info");
            ret.commit_info = commitInfo;
        }
        else
        {
            throw std::runtime_error("WalFile: read: unknown type " + std::to_string(static_cast<int>(type)));
        }
    }

    return ret;
}

bool WalFile::sync(const CommitInfo& commitInfo)
{
    CWData data;
    writeDataHeader(data, dataTypeSync, seqNo);
    serializeCommitInfo(commitInfo, data);

    if(!writeData(data))
        return false;

    return currentFile().fsyncNoInt() == 0;
}

bool WalFile::fsync()
{
    return currentFile().fsyncNoInt() == 0;
}

bool WalFile::switchFiles()
{
    std::scoped_lock lock(mutex);
    if(currentFile().fsyncNoInt() != 0)
    {
        XLOGF(ERR, "WalFile: switchFiles: failed to sync WAL file: {}", folly::errnoStr(errno));
        return false;
    }

    DataItem item;
    item.dataOff = 0;
    item.isAlt = useAltFile;
    dataWriteQueue.push(std::move(item));
    dataWriteCond.notify_one();
    incrPendingData();

    useAltFile = !useAltFile;
    pendingReset = true;
    ResetPrep prep(nullptr);
    reset(prep, true, std::nullopt);

    return true;
}

void WalFile::reset(ResetPrep& prep, const bool sync, const std::optional<bool> useAltFileManual)
{
    const bool writeHeader = !useAltFileManual;
    if(writeHeader)
    {
        offset = 0;
        _items = 0;
        ++seqNo;
    }

    File& file = useAltFileManual ? currentFileManual(*useAltFileManual) : currentFile();

    file.truncate(0);

    if(writeHeader)
    {
        CWData data;
        writeDataHeader(data, dataTypeInit, seqNo);

        data.addString2(walFileInitString);
        data.addString2(walUuid);

        setChecksumAndSize(data);


        if(file.pwriteFull(data.getDataPtr(), data.getDataSize(), 0) != data.getDataSize())
        {
            XLOGF(ERR, "WalFile: failed to write initial data after reset: {}", folly::errnoStr(errno));
            abort();
        }

        offset += data.getDataSize();
    }

    if(sync)
    {
        if(file.fsyncNoInt() != 0)
        {
            XLOGF(ERR, "WalFile: reset: failed to sync WAL file: {}", folly::errnoStr(errno));
            abort();
        }
    }

    prep.unlock();
}

bool WalFile::writeData(const int64_t off, const char* data, const size_t dataSize, const bool useThreadWrite)
{
    CWData wdata;
    writeDataHeader(wdata, dataTypeDataFileData, seqNo);
    wdata.addVarInt(off);
    wdata.addBuffer(data, dataSize);

    if(useThreadWrite)
    {
        DataItem item;
        item.dataOff = off;
        item.data.assign(data, data + dataSize);

        if(!writeData(wdata, &item))
            return false;
    }
    else
    {
        if(!writeData(wdata))
            return false;
    }
    
    return true;
}

void WalFile::waitForWrites()
{
    std::unique_lock lock(mutex);
    while(altPendingData())
    {
        pendingDataCond.wait(lock);
    }
}

void WalFile::dataWriteThread(std::stop_token stopToken)
{
    folly::setThreadName("Wal data writer");

    std::unique_lock lock(mutex);

    while(true)
    {
        while(dataWriteQueue.empty() && !stopToken.stop_requested())
        {
            dataWriteCond.wait(lock);
        }

        if(stopToken.stop_requested())
            break;

        auto item = std::move(dataWriteQueue.front());
        dataWriteQueue.pop();

        const auto wasEmpty = dataWriteQueue.empty();

        lock.unlock();

        if(wasEmpty && FLAGS_wal_write_delay)
        {
            // Introduce artificial delay for testing
            const auto delayMs = folly::Random::rand32(50, 150);
            XLOGF(INFO, "WalFile: dataWriteThread: delaying WAL data write by {} ms for testing", delayMs);
            std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        }

        XLOGF(DBG0, "WalFile: dataWriteThread: writing data {} to data file at offset {} size {}", folly::crc32c(reinterpret_cast<const uint8_t*>(item.data.data()), item.data.size()), item.dataOff, item.data.size());

        if(!item.data.empty() &&
            dataFile.pwriteFull(item.data.data(), item.data.size(), item.dataOff) != item.data.size())
        {
            XLOGF(ERR, "WalFile: dataWriteThread: failed to write data to data file: {}", folly::errnoStr(errno));
            abort();
        }

        lock.lock();

        if(!item.data.empty())
        {
            forEachDataItem(item.dataOff, item.data.size(), [&](const int64_t block)
            {
                auto it = dataItems.find(block);
                assert(it != dataItems.end());
                if(it!=dataItems.end() &&
                    --it->second.refs == 0)
                {
                    dataItems.erase(it);
                }
                return false;
            });

            writeDoneCond.notify_all();
        }        

        if(lastPendingData(item.isAlt))
        {
            if(pendingReset)
            {
                assert(item.dataOff == 0 && item.data.empty());
                lock.unlock();

                XLOGF(INFO, "WalFile: dataWriteThread: syncing data file");

                if(dataFile.fsyncNoInt() != 0)
                {
                    XLOGF(ERR, "WalFile: dataWriteThread: failed to sync data file: {}", folly::errnoStr(errno));
                    abort();
                }

                lock.lock();

                ResetPrep prep(nullptr);
                reset(prep, true, item.isAlt);
                pendingReset = false;
            }
            else
            {
                assert(item.dataOff != 0 || !item.data.empty());
            }

            const bool wasLast = decrPendingData(item.isAlt);
            assert(wasLast);

            pendingDataCond.notify_all();
        }
        else if(item.dataOff == 0 && item.data.empty() && pendingReset)
        {
            assert(false);
            decrPendingData(item.isAlt);
        }
        else
        {            
            decrPendingData(item.isAlt);
        }
    }

}

void WalFile::waitForWriteout(const int64_t off, const size_t dataSize)
{
    std::unique_lock lock(mutex);
    while(true)
    {
        auto wait = forEachDataItem(off, dataSize, [&](const int64_t block)
        {
            auto it = dataItems.find(block);
            if(it!=dataItems.end())
            {
                XLOGF(DBG0, "WalFile: waitForWriteout: waiting for {} for read offset {} size {} to be written out", block*waitBlockSize, off, dataSize);
                return true;
            }

            return false;
        });
        
        if(wait)
        {
            writeDoneCond.wait(lock);
            continue;
        }

        return;
    }
}