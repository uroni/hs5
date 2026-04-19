#pragma once

#include <string>
#include <optional>
#include "apigen/ListResp.hpp"

namespace buckets
{

void refreshBucketCache();

int64_t addBucket(const std::string_view bucketName, bool failIfAlreadyExists);

bool isValidBucketName(const std::string_view bucketName);

std::optional<int64_t> getBucket(const std::string_view bucketName);

enum class VersioningState
{
    Disabled = 0,
    Enabled = 1,
    Suspended = 2
};

std::optional<VersioningState> versioningStateFromStr(const std::string_view str);

std::string versioningStateToStr(VersioningState state);

struct BucketInfo
{
    int64_t id;
    std::chrono::seconds created;
    int publicPerms = 0;
    VersioningState versioning = VersioningState::Disabled;
};

std::optional<BucketInfo> getBucketInfo(const std::string_view bucketName);

bool deleteBucket(int64_t bucketId);

int64_t getPartialUploadsBucket(int64_t bucketId);

bool isPartialUploadsBucket(int64_t bucketId);

int64_t getPartsBucket(int64_t bucketId);

bool isPartsBucket(int64_t bucketId);

std::string getBucketName(int64_t bucketId);

Api::ListResp getBucketNames();

bool setVersioning(const std::string_view bucketName, VersioningState versioningState);

} // namespace buckets