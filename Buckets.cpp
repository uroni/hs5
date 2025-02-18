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

namespace
{
    std::map<std::string, int64_t> buckets;
    std::map<int64_t, std::map<std::string, int64_t>::iterator> bucketNames;
    int64_t currMaxId = 1;
    std::shared_mutex mutex;

    int64_t nextId()
    {
        auto ret = currMaxId;
        currMaxId+=2;
        return ret;
    }
}

void refreshBucketCache()
{
    DbDao dao;

    std::scoped_lock lock{mutex};

    bucketNames.clear();
    buckets.clear();
    currMaxId = dao.getMaxBucketId() + 1;

    XLOGF(INFO, "Init max bucket id {}", currMaxId);

    const auto dbBuckets = dao.getBuckets();

    for(const auto& bucket: dbBuckets)
    {
        auto ins = buckets.insert(std::make_pair(bucket.name, bucket.id));
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

        return it->second;
    }

    const auto id = nextId();

    dao.addBucket(id, std::string(bucketName));

    auto ins = buckets.insert(std::make_pair(std::string(bucketName), id));
    bucketNames.insert(std::make_pair(id, ins.first));

    return id;
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
    return it->second;
}

int64_t getPartialUploadsBucket(int64_t bucketId)
{
    // Create documentation
    return bucketId + 1;
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

    resp.objects.reserve(bucketNames.size());

    for(auto it=bucketNames.begin();it!=bucketNames.end();++it)
    {
        Api::Object obj;
        obj.name = it->second->second;
        obj.type = 0;
        resp.objects.emplace_back(std::move(obj));
    }

    resp.isTruncated = false;

    return resp;
}