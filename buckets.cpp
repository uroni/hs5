#include "buckets.h"
#include <mutex>

const bool autogenBuckets = true;


std::optional<int64_t> Buckets::getBucket(const std::string_view bucketName)
{
    std::scoped_lock lock{mutex};

    auto it = buckets.find(std::string(bucketName));
    if(it==buckets.end())
    {
        if(autogenBuckets)
        {
            const auto id = nextId();
            auto ins = buckets.insert(std::make_pair(std::string(bucketName), id));
            bucketNames.insert(std::make_pair(id, ins.first));
            return id;
        }
        return {};
    }
    return it->second;
}

int64_t Buckets::getPartialUploadsBucket(int64_t bucketId)
{
    return bucketId + 1;
}

std::string Buckets::getBucketName(int64_t bucketId)
{
    std::scoped_lock lock{mutex};

    auto it = bucketNames.find(bucketId);
    if(it==bucketNames.end())
        return {};

    return it->second->first;
}

int64_t Buckets::nextId()
{
    auto ret = currMaxId;
    currMaxId+=2;
    return ret;
}