#pragma once

#include <string>
#include <optional>
#include "apigen/ListResp.hpp"

void refreshBucketCache();

int64_t addBucket(const std::string_view bucketName, bool failIfAlreadyExists);

std::optional<int64_t> getBucket(const std::string_view bucketName);

int64_t getPartialUploadsBucket(int64_t bucketId);

std::string getBucketName(int64_t bucketId);

Api::ListResp getBucketNames();