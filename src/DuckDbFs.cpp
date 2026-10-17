/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "DuckDbFs.h"
#include "Buckets.h"
#include "s3handler.h"
#include <duckdb/common/string_util.hpp>
#include <folly/ScopeGuard.h>

// TODO: Replace with #include <duckdb/function/scalar/string_common.hpp>
namespace duckdb
{
    DUCKDB_API bool Glob(const char* s, idx_t slen, const char* pattern, idx_t plen, bool allow_question_mark = true);
}

DuckDbFileHandle::DuckDbFileHandle(duckdb::FileSystem& fs, std::string path, duckdb::FileOpenFlags flags, std::unique_ptr<ObjectReader> reader)
    : duckdb::FileHandle(fs, path, flags), reader(std::move(reader))
{
}

DuckDbFileHandle::~DuckDbFileHandle()
{
    Close();
}

void DuckDbFileHandle::Close()
{
    if (reader)
        reader->Close();
}

ObjectReader& DuckDbFileHandle::readerOrThrow()
{
    if (!reader)
        throw duckdb::IOException("HS5 file not opened for reading: " + path);
    return *reader;
}

void DuckDbFileHandle::Read(char* buffer, int64_t bsize, int64_t offset)
{
    try
    {
        readerOrThrow().Read(buffer, bsize, offset);
    }
    catch (const ObjectReadError& e)
    {
        throw duckdb::IOException(e.what());
    }
}

int64_t DuckDbFileHandle::Read(char* buffer, int64_t bsize)
{
    try
    {
        return readerOrThrow().Read(buffer, bsize);
    }
    catch (const ObjectReadError& e)
    {
        throw duckdb::IOException(e.what());
    }
}

void DuckDbFs::RemoveDirectory(const std::string& path, duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    ListFiles(
        path,
        [&](const std::string& file, bool is_dir)
        {
            try
            {
                this->RemoveFile(file, opener);
            }
            catch (duckdb::IOException& e)
            {
                std::string errmsg(e.what());
                if (errmsg.find("No such file or directory") != std::string::npos)
                {
                    return;
                }
                throw;
            }
        },
        opener.get());
}

bool DuckDbFs::ListFiles(const std::string& directory, const std::function<void(const std::string&, bool)>& callback,
                         duckdb::FileOpener* opener)
{
    std::string trimmed_dir = directory;
    duckdb::StringUtil::RTrim(trimmed_dir, PathSeparator(trimmed_dir));
    auto glob_res = Glob(JoinPath(trimmed_dir, "**"), opener);

    if (glob_res.empty())
    {
        return false;
    }

    for (const auto& file : glob_res)
    {
        callback(file.path, false);
    }

    return true;
}

static std::string GetPrefix(std::string url)
{
    const std::string prefixes[] = { "hs5://" };
    for (auto& prefix : prefixes)
    {
        if (duckdb::StringUtil::StartsWith(url, prefix))
        {
            return prefix;
        }
    }
    throw duckdb::IOException("URL needs to start with hs5://");
}

static bool Match(std::vector<std::string>::const_iterator key, std::vector<std::string>::const_iterator key_end,
                  std::vector<std::string>::const_iterator pattern, std::vector<std::string>::const_iterator pattern_end)
{

    while (key != key_end && pattern != pattern_end)
    {
        if (*pattern == "**")
        {
            if (std::next(pattern) == pattern_end)
            {
                return true;
            }
            while (key != key_end)
            {
                if (Match(key, key_end, std::next(pattern), pattern_end))
                {
                    return true;
                }
                key++;
            }
            return false;
        }
        if (!duckdb::Glob(key->data(), key->length(), pattern->data(), pattern->length()))
        {
            return false;
        }
        key++;
        pattern++;
    }
    return key == key_end && pattern == pattern_end;
}

ParsedHs5Url DuckDbFs::Hs5UrlParse(std::string url)
{
    const auto prefix = GetPrefix(url);
    const auto prefix_end_pos = url.find("//") + 2;
    const auto slash_pos = url.find('/', prefix_end_pos);
    if (slash_pos == std::string::npos)
    {
        throw duckdb::IOException("URL needs to contain a '/' after the host");
    }

    const auto bucket = url.substr(prefix_end_pos, slash_pos - prefix_end_pos);

    if (bucket.empty())
    {
        throw duckdb::IOException("URL needs to contain a bucket name");
    }

    const auto key = url.substr(slash_pos + 1);

    return { prefix, bucket, key };
}

duckdb::vector<duckdb::OpenFileInfo> DuckDbFs::Glob(const std::string& glob_pattern, duckdb::FileOpener* opener)
{
    const auto parsedUrl = Hs5UrlParse(glob_pattern);

    const auto firstWildcardPos = parsedUrl.key.find_first_of("*[\\");
    if (firstWildcardPos == std::string::npos)
    {
        return { glob_pattern };
    }

    auto bucketId = buckets::getBucket(parsedUrl.bucket);
    if (!bucketId)
    {
        throw duckdb::IOException("Bucket not found");
    }

    const auto sharedPath = std::string_view(parsedUrl.key).substr(0, firstWildcardPos);
    const auto patternSplits = duckdb::StringUtil::Split(parsedUrl.key, "/");

    const auto iterStartVal = make_key(sharedPath, *bucketId, std::numeric_limits<int64_t>::max());

    sfs.list_commit();

    SingleFileStorage::IterData iterData;

    if (!sfs.iter_start(iterStartVal, false, iterData))
    {
        throw duckdb::IOException("Failed to start listing");
    }

    auto guard = folly::makeGuard([&]
                                  { sfs.iter_stop(iterData); });

    std::string lastOutputKeyStr;

    duckdb::vector<duckdb::OpenFileInfo> ret;

    while (true)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if (!sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iterData))
        {
            break;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);

        if (!keyInfo.key.starts_with(sharedPath))
        {
            break;
        }

        if (keyInfo.bucketId != bucketId)
        {
            break;
        }

        bool outputKey = true;

        if (keyInfo.key == lastOutputKeyStr)
        {
            outputKey = false;
        }
        else if (md5sum.size() == 1 && md5sum[0] == metadata_tombstone)
        {
            lastOutputKeyStr = keyInfo.key;
            outputKey = false;
        }

        if (outputKey)
        {
            lastOutputKeyStr = keyInfo.key;

            const auto keySplits = duckdb::StringUtil::Split(std::string(keyInfo.key), "/");
            const auto isMatch = Match(keySplits.begin(), keySplits.end(), patternSplits.begin(), patternSplits.end());
            if (isMatch)
            {
                ret.emplace_back(parsedUrl.http_proto + parsedUrl.bucket + "/" + std::string(keyInfo.key));
            }
        }

        if (!sfs.iter_next(iterData))
        {
            throw duckdb::IOException("Failed to list next");
        }
    }

    return ret;
}

void DuckDbFs::Read(duckdb::FileHandle& handle, void* buffer, int64_t nr_bytes, duckdb::idx_t location)
{
    handle.Cast<DuckDbFileHandle>().Read(static_cast<char*>(buffer), nr_bytes, location);
}

int64_t DuckDbFs::Read(duckdb::FileHandle& handle, void* buffer, int64_t nr_bytes)
{
    return handle.Cast<DuckDbFileHandle>().Read(static_cast<char*>(buffer), nr_bytes);
}

bool DuckDbFs::CanHandleFile(const std::string& fpath)
{
    return fpath.starts_with("hs5://");
}

duckdb::unique_ptr<duckdb::FileHandle> DuckDbFs::OpenFile(const std::string& path, duckdb::FileOpenFlags flags,
                                                          duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    if (flags.OpenForReading() && flags.OpenForWriting())
    {
        throw duckdb::NotImplementedException("Cannot open an HS5 file for both reading and writing");
    }
    else if (flags.OpenForAppending())
    {
        throw duckdb::NotImplementedException("Cannot open an HS5 file for appending");
    }

    const auto parsedUrl = Hs5UrlParse(path);

    std::unique_ptr<ObjectReader> reader;
    if (flags.OpenForReading())
    {
        try
        {
            reader = std::make_unique<ObjectReader>(sfs, parsedUrl.bucket, parsedUrl.key);
        }
        catch (const ObjectReadError& e)
        {
            if (e.notFound() && flags.ReturnNullIfNotExists())
                return nullptr;
            throw duckdb::IOException(e.what());
        }
    }
    else if (!buckets::getBucketInfo(parsedUrl.bucket))
    {
        throw duckdb::IOException("Bucket not found");
    }

    return duckdb::make_uniq<DuckDbFileHandle>(*this, path, flags, std::move(reader));
}

int64_t DuckDbFs::GetFileSize(duckdb::FileHandle& handle)
{
    return handle.Cast<DuckDbFileHandle>().FileSize();
}

time_t DuckDbFs::GetLastModifiedTime(duckdb::FileHandle& handle)
{
    return handle.Cast<DuckDbFileHandle>().LastModifiedTime();
}