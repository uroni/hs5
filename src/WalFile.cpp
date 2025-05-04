/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "WalFile.h"
#include "data.h"
#include <folly/system/MemoryMapping.h>
#include <folly/hash/Checksum.h>

WalFile::WalFile(const std::string &path)
    : file(path, O_RDWR | O_CREAT | O_APPEND, 0644)
{
    
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
    data.addUInt(0); // checksum
    data.addUInt(0); // size
    data.addChar(0); // version
    data.addVarInt(transid);
    data.addUChar(static_cast<unsigned char>(info.action));
    data.addString2(info.fn);
    data.addVarInt(info.offset);
    data.addVarInt(info.len);
    data.addVarInt(info.last_modified);
    data.addVarInt(info.extra_exts.size());
    for (const auto& ext : info.extra_exts)
    {
        data.addVarInt(ext.offset);
        data.addVarInt(ext.len);
    }

    const auto dsize = static_cast<unsigned int>(data.getDataSize());
    memcpy(data.getDataPtr() + sizeof(dsize), &dsize, sizeof(dsize));
    const auto crc = folly::crc32c(reinterpret_cast<uint8_t*>(data.getDataPtr() + sizeof(dsize)), dsize - sizeof(dsize));    
    static_assert(sizeof(crc) == sizeof(unsigned int));
    memcpy(data.getDataPtr(), &crc, sizeof(crc));

    if(file.pwriteFull(data.getDataPtr(), data.getDataSize(), offset) != data.getDataSize())
    {
        return false;
    }

    ++_items;

    offset += data.getDataSize();
    return true;
}

std::vector<SingleFileStorage::SFragInfo> WalFile::read(int64_t transid)
{
    folly::MemoryMapping mapping(file.dupCloseOnExec());

    const auto allData = mapping.range();
    size_t off = 0;
    std::vector<SingleFileStorage::SFragInfo> ret;
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
        int64_t ctransid;
        data.getVarInt(&ctransid);
        if(ctransid != transid)
        {            
            continue;
        }
        SingleFileStorage::SFragInfo info;
        unsigned char action;
        data.getUChar(&action);
        info.action = static_cast<SingleFileStorage::FragAction>(action);
        data.getStr2(&info.fn);
        data.getVarInt(&info.offset);
        data.getVarInt(&info.len);
        data.getVarInt(&info.last_modified);
        
        int64_t extra_exts_size = 0;
        data.getVarInt(&extra_exts_size);
        
        for(size_t i = 0; i < extra_exts_size; ++i)
        {
            SingleFileStorage::SPunchItem ext;
            data.getVarInt(&ext.offset);
            data.getVarInt(&ext.len);
            info.extra_exts.push_back(ext);
        }

        ret.push_back(info);
    }

    return ret;
}

bool WalFile::sync()
{
    return file.fsyncNoInt() == 0;
}

void WalFile::reset()
{
    offset = 0;
    _items = 0;
    file.truncate(0);
}