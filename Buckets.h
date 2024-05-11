#pragma once

#include <string>
#include <optional>

void refreshBucketCache();

std::optional<int64_t> getBucket(const std::string_view bucketName);

int64_t getPartialUploadsBucket(int64_t bucketId);

std::string getBucketName(int64_t bucketId);

