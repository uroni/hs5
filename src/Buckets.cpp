/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
 #include "Buckets.h"
#include "DbDao.h"
#include "apigen/Object.hpp"
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <gflags/gflags.h>
#include <folly/logging/xlog.h>

DEFINE_bool(autogen_buckets, false, "Automatically create buckets when they are used");

namespace buckets
{

namespace
{
    struct BucketInfo
    {
        int64_t id;
        std::chrono::seconds created;
    };

    std::map<std::string, BucketInfo> buckets;
    std::map<int64_t, std::map<std::string, BucketInfo>::iterator> bucketNames;
    int64_t currMaxId = 1;
    std::shared_mutex mutex;

    int64_t nextId()
    {
        auto ret = currMaxId;
        currMaxId+=4;
        return ret;
    }
}

void refreshBucketCache()
{
    DbDao dao;

    std::scoped_lock lock{mutex};

    bucketNames.clear();
    buckets.clear();
    const auto dbBuckets = dao.getBuckets();

    if(!dbBuckets.empty())
    {
        currMaxId = dao.getMaxBucketId();
        nextId();    
    }

    XLOGF(INFO, "Init max bucket id {}", currMaxId);

    for(const auto& bucket: dbBuckets)
    {
        BucketInfo info{
            .id = bucket.id,
            .created = std::chrono::seconds(bucket.created)
        };
        auto ins = buckets.insert(std::make_pair(bucket.name, info));
        bucketNames.insert(std::make_pair(bucket.id, ins.first));
    }   
}

int64_t addBucket(const std::string_view bucketName, bool failIfAlreadyExists)
{
    DbDao dao;

    std::scoped_lock lock{mutex};

    auto it = buckets.find(std::string(bucketName));
    if(it!=buckets.end())
    {
        if(failIfAlreadyExists)
            return -1;

        return it->second.id;
    }

    const auto id = nextId();

    BucketInfo info{
        .id = id,
        .created = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch())
    };

    dao.addBucket(id, std::string(bucketName), info.created.count());

    auto ins = buckets.insert(std::make_pair(std::string(bucketName), info));
    bucketNames.insert(std::make_pair(id, ins.first));

    return id;
}

bool isValidBucketName(const std::string_view bucketName)
{
    if(bucketName.size()<3 || bucketName.size()>63)
        return false;

    if(bucketName.starts_with("xn--") || bucketName.starts_with("sthree-") || bucketName.starts_with("amzn-s3-demo-") 
        || bucketName.ends_with("-s3alias") || bucketName.ends_with("--ol-s3") || bucketName.ends_with(".mrap") || bucketName.ends_with("--x-s3") 
        || bucketName.ends_with("--table-s3") )
        return false;

    // TODO: Check for IP address like names (e.g. 192.168.5.4)

    size_t lastPeriod = std::string_view::npos;
    for(size_t i=0;i<bucketName.size();++i)
    {
        const auto c = bucketName[i];
        if(i==0 || i == bucketName.size()-1)
        {
            if((c>='a' && c<='z') || (c>='0' && c<='9'))
                continue;
            return false;
        }
        else if((c>='a' && c<='z') || (c>='0' && c<='9') || c=='-' || c=='.')
        {
            if(c=='.')
            {
                if(lastPeriod != std::string_view::npos && i-1 == lastPeriod)
                    return false;
                lastPeriod = i;
            }
            continue;
        }
        return false;
    }
    return true;
}

std::optional<int64_t> getBucket(const std::string_view bucketName)
{
    std::unique_lock lock{mutex};

    auto it = buckets.find(std::string(bucketName));
    if(it==buckets.end())
    {
        if(FLAGS_autogen_buckets)
        {
            lock.unlock();
            return addBucket(bucketName, false);
        }
        return {};
    }
    return it->second.id;
}

bool deleteBucket(int64_t bucketId)
{
    DbDao dao;

    std::scoped_lock lock{mutex};

    auto it = bucketNames.find(bucketId);
    if(it==bucketNames.end())
        return false;

    dao.deleteBucket(bucketId);

    buckets.erase(it->second->first);
    bucketNames.erase(it);

    return true;
}

int64_t getPartialUploadsBucket(int64_t bucketId)
{
    // Create documentation
    return bucketId + 1;
}

bool isPartialUploadsBucket(int64_t bucketId)
{
    return bucketId % 4 == 2;
}

int64_t getPartsBucket(int64_t bucketId)
{
    return bucketId + 2;
}

bool isPartsBucket(int64_t bucketId)
{
    return bucketId % 4 == 3;
}

std::string getBucketName(int64_t bucketId)
{
    std::scoped_lock lock{mutex};

    auto it = bucketNames.find(bucketId);
    if(it==bucketNames.end())
        return {};

    return it->second->first;
}

Api::ListResp getBucketNames()
{
    Api::ListResp resp;
    std::scoped_lock lock{mutex};

    resp.objects.reserve(buckets.size());

    for(const auto& bucket: buckets)
    {
        Api::Object obj;
        obj.name = bucket.first;
        obj.created = std::chrono::nanoseconds(bucket.second.created).count();
        obj.type = 0;
        resp.objects.emplace_back(std::move(obj));
    }

    resp.isTruncated = false;

    return resp;
}

} // namespace buckets