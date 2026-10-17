#pragma once

#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include "SingleFileStorage.h"
#include "ObjMetadata.h"
#include "PayloadHash.h"

struct MultiPartDownloadData;

class ObjectReadError : public std::runtime_error {
public:
    ObjectReadError(const std::string& msg, bool notFound = false)
     : std::runtime_error(msg), notFound_(notFound) {}

    bool notFound() const {
        return notFound_;
    }

private:
    bool notFound_;
};

// Reads the newest version of an object (single or multipart). Throws ObjectReadError on failure
class ObjectReader {
public:
    ObjectReader(SingleFileStorage& sfs, const std::string& bucket, const std::string& key);

    ~ObjectReader();

    ObjectReader(const ObjectReader&) = delete;
    ObjectReader& operator=(const ObjectReader&) = delete;

    void Close();

    void Read(char* buffer, int64_t bsize, int64_t offset);

    int64_t Read(char *buffer, int64_t bsize);

    int64_t FileSize() const {
        return fsize;
    }

    time_t LastModifiedTime() const;

    int64_t LastModifiedTimeNs() const {
        return lastModified;
    }

    std::string ETag() const;

    ObjMetadata* GetObjMetadata() {
        return objMetadata.get();
    }

    PayloadHashBase* GetSdkChecksumHash() {
        return sdkChecksumHash.get();
    }

private:
    SingleFileStorage& sfs;

    // bucket/key, for messages
    std::string name;

    int64_t bucketId = 0;

    int64_t fsize = 0;

    int64_t lastModified = 0;

    std::string etag;

    int64_t pos = 0;

    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;

    std::vector<SingleFileStorage::Ext> extents;

    bool open = false;
    std::string s3key;

    std::unique_ptr<ObjMetadata> objMetadata;
    std::unique_ptr<PayloadHashBase> sdkChecksumHash;
};
