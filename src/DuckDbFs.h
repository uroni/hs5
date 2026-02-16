#pragma once

#include <duckdb/common/file_system.hpp>
#include <string>
#include "SingleFileStorage.h"

struct MultiPartDownloadData;

class DuckDbFileHandle : public duckdb::FileHandle {
public:
    DuckDbFileHandle(duckdb::FileSystem &fs, std::string path, duckdb::FileOpenFlags flags);

    ~DuckDbFileHandle();;

    void Initialize();

    void Read(char* buffer, int64_t bsize, int64_t offset);

    int64_t Read(char *buffer, int64_t bsize);

    void Close() override ;

    int64_t FileSize() {
        return fsize;
    }

    time_t LastModifiedTime();

private:
    struct PartExt
    {
        int64_t size;
        int start;
        int len;
    };

    int64_t bucketId = 0;

    int64_t fsize = 0;

    int64_t lastModified = 0;

    int64_t pos = 0;

    std::unique_ptr<MultiPartDownloadData> multiPartDownloadData;

    SingleFileStorage& sfs();

    std::vector<SingleFileStorage::Ext> extents;

    bool open = false;

    
};

struct ParsedHs5Url {
	const std::string http_proto;
	const std::string bucket;
	const std::string key;
};

class DuckDbFs : public duckdb::FileSystem {
public:
    friend DuckDbFileHandle;

    DuckDbFs(SingleFileStorage& sfs, const bool withBucketVersioning) : sfs(sfs), withBucketVersioning(withBucketVersioning) {
    }

    bool CanSeek() override {
		return true;
	}
	bool OnDiskFile(duckdb::FileHandle &handle) override {
		return false;
	}
	bool IsPipe(const std::string &filename, duckdb::optional_ptr<duckdb::FileOpener> opener) override {
		return false;
	}

    std::string GetName() const override {
		return "Hs5FileSystem";
	}

	std::string PathSeparator(const std::string &path) override {
		return "/";
	}

    void RemoveDirectory(const std::string &path, duckdb::optional_ptr<duckdb::FileOpener> opener) override;

    bool ListFiles(const std::string &directory, const std::function<void(const std::string &, bool)> &callback,
                             duckdb::FileOpener *opener) override;


    duckdb::vector<duckdb::OpenFileInfo> Glob(const std::string &glob_pattern, duckdb::FileOpener *opener) override;

    static ParsedHs5Url Hs5UrlParse(std::string url);

    void Read(duckdb::FileHandle &handle, void *buffer, int64_t nr_bytes, duckdb::idx_t location) override;

    int64_t Read(duckdb::FileHandle &handle, void *buffer, int64_t nr_bytes) override;

    bool CanHandleFile(const std::string &fpath) override;

    duckdb::unique_ptr<duckdb::FileHandle> OpenFile(const std::string &path, duckdb::FileOpenFlags flags,
        duckdb::optional_ptr<duckdb::FileOpener> opener = nullptr) override;


    bool isWithBucketVersioning() const {
        return withBucketVersioning;
    }

    virtual int64_t GetFileSize(duckdb::FileHandle &handle);

    virtual time_t GetLastModifiedTime(duckdb::FileHandle &handle) override;
private:
    SingleFileStorage& sfs;
    const bool withBucketVersioning;
};

