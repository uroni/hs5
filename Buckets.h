#pragma once

#include <string>
#include <optional>
#include <map>
#include <mutex>

class Buckets
{
public:
    std::optional<int64_t> getBucket(const std::string_view bucketName);

    int64_t getPartialUploadsBucket(int64_t bucketId);

    std::string getBucketName(int64_t bucketId);

private:
    int64_t nextId();

    std::map<std::string, int64_t> buckets;
    std::map<int64_t, std::map<std::string, int64_t>::iterator> bucketNames;
    int64_t currMaxId = 1;
    std::mutex mutex;
};