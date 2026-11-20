#pragma once

#include "ObjMetadata.h"
#include "PayloadHash.h"
#include "SingleFileStorage.h"
#include <ctime>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

struct MultiPartDownloadData;

class ObjectReadError : public std::runtime_error
{
public:
    ObjectReadError(const std::string& msg, bool notFound = false, bool deleteMarker = false)
        : std::runtime_error(msg), notFound_(notFound), deleteMarker_(deleteMarker) {}

    bool notFound() const
    {
        return notFound_;
    }

    // The explicitly requested version is a delete marker
    bool deleteMarker() const
    {
        return deleteMarker_;
    }

private:
    bool notFound_;
    bool deleteMarker_;
};

// Reads the newest (or the given) version of an object (single or multipart). Throws ObjectReadError on failure
class ObjectReader
{
public:
    enum class VersionSelect
    {
        // Given version. Newest version if version is 0 and the bucket is versioned
        Default,
        // Exactly the given version (also if it is 0)
        Exact,
        // Oldest version (version is ignored)
        Oldest
    };

    // readFlags are flags for reading data (SingleFileStorage::read_ext), e.g. SingleFileStorage::ReadWithMmap.
    // prepareFlags are additional flags for SingleFileStorage::read_prepare, e.g. SingleFileStorage::ReadFailUnsynced
    ObjectReader(SingleFileStorage& sfs, const std::string& bucket, const std::string& key, const int64_t version = 0,
                 const VersionSelect versionSelect = VersionSelect::Default, const unsigned int readFlags = 0,
                 const unsigned int prepareFlags = 0);

    ~ObjectReader();

    ObjectReader(const ObjectReader&) = delete;
    ObjectReader& operator=(const ObjectReader&) = delete;

    void Close();

    void Read(char* buffer, int64_t bsize, int64_t offset);

    int64_t Read(char* buffer, int64_t bsize);

    int64_t FileSize() const
    {
        return fsize;
    }

    time_t LastModifiedTime() const;

    int64_t LastModifiedTimeNs() const
    {
        return lastModified;
    }

    std::string ETag() const;

    // Version of the read object (0 if unversioned)
    int64_t Version() const;

    ObjMetadata* GetObjMetadata()
    {
        return objMetadata.get();
    }

    PayloadHashBase* GetSdkChecksumHash()
    {
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

    const unsigned int readFlags;

    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;

    std::vector<SingleFileStorage::Ext> extents;

    bool open = false;
    std::string s3key;

    std::unique_ptr<ObjMetadata> objMetadata;
    std::unique_ptr<PayloadHashBase> sdkChecksumHash;
};
