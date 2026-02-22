/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "DuckDbFs.h"
#include <duckdb/common/string_util.hpp>
#include "Buckets.h"
#include "s3handler.h"
#include <folly/ScopeGuard.h>

// TODO: Replace with #include <duckdb/function/scalar/string_common.hpp>
namespace duckdb {
    DUCKDB_API bool Glob(const char *s, idx_t slen, const char *pattern, idx_t plen, bool allow_question_mark = true);
}

DuckDbFileHandle::DuckDbFileHandle(duckdb::FileSystem &fs, std::string path, duckdb::FileOpenFlags flags)
 : duckdb::FileHandle(fs, path, flags)
{
    if (flags.OpenForReading() && flags.OpenForWriting()) {
        throw duckdb::NotImplementedException("Cannot open an HS5 file for both reading and writing");
    } else if (flags.OpenForAppending()) {
        throw duckdb::NotImplementedException("Cannot open an HS5 file for appending");
    }

    Initialize();

    open = true;
}

DuckDbFileHandle::~DuckDbFileHandle()
{
    Close();
}

void DuckDbFileHandle::Close()
{
    if(!open)
        return;
    
    if(flags.OpenForReading())
    {
        if(multiPartDownloadData)
        {
            const auto rc = S3Handler::finalizeMultiPart(sfs(), s3key, bucketId, *multiPartDownloadData, extents);
            if(rc)
                XLOGF(WARN, "Error finalizing multipart download: {}", rc);
        }
        else
        {
            sfs().read_finalize(s3key, extents, 0);
        }
    }

    open = false;
}

void DuckDbFileHandle::Initialize()
{
    const auto parsedUrl = DuckDbFs::Hs5UrlParse(path);

    const auto bucketIdOpt = buckets::getBucket(parsedUrl.bucket);
    if(!bucketIdOpt)
    {
        throw duckdb::IOException("Bucket not found");
    }

    bucketId = *bucketIdOpt;

    unsigned int readPrepFlags = 0;

    const auto withVersioning = static_cast<DuckDbFs&>(file_system).isWithBucketVersioning();
    if(withVersioning)
    {
        s3key = make_key(parsedUrl.key, bucketId, std::numeric_limits<int64_t>::max());
        readPrepFlags |= SingleFileStorage::ReadNewest;
    }
    else
    {
        s3key = make_key(parsedUrl.key, bucketId, 0);
    }

    if (flags.OpenForReading())
    {
        auto res = sfs().read_prepare(s3key, readPrepFlags);

        if (res.err != 0)
        {
            if(res.err==ENOENT)
            {
                throw duckdb::IOException("File not found: " + path);
            }
            throw duckdb::IOException("Failed to prepare read: " + std::to_string(res.err));
        }

        if(withVersioning)
            s3key = res.key;

        if(!S3Handler::parseMultipartInfo(res.md5sum, res.total_len, multiPartDownloadData, nullptr))
        {
            throw duckdb::IOException("Failed to parse multipart info for file: " + path);
        }

        if(res.md5sum.size()==1 && res.md5sum[0] == metadata_tombstone)
        {
            throw duckdb::IOException("File not found: " + path);
        }

        extents = std::move(res.extents);
        fsize = res.total_len;
        lastModified = res.last_modified;
    }
}

SingleFileStorage& DuckDbFileHandle::sfs()
{
    return static_cast<DuckDbFs&>(file_system).sfs;
}

void DuckDbFileHandle::Read(char* buffer, int64_t bsize, int64_t offset)
{
    if(multiPartDownloadData)
    {
        const auto rc = S3Handler::seekMultipartExt(sfs(), offset, bucketId, *multiPartDownloadData, extents);
        if(rc)
        {
            throw duckdb::IOException("Error seeking to part rc " + std::to_string(rc));
        }
    }

    while(offset < fsize && bsize > 0)
    {
        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if(!extents.empty())
            --it;
        assert(it != extents.end());
        if(it==extents.end() || it->len==0 || it->obj_offset + it->len <= offset)
        {
            if(multiPartDownloadData)
            {
                const int rc = S3Handler::readNextMultipartExt(sfs(), offset, bucketId, *multiPartDownloadData, extents);
                if(rc)
                {
                    throw duckdb::IOException("Error reading next part code " + std::to_string(rc));
                }
                continue;
            }
            break;
        }

        assert(it->obj_offset <= offset && it->obj_offset + it->len > offset);
        
        int64_t ext_offset = offset - it->obj_offset;
        auto curr_ext = SingleFileStorage::Ext(it->obj_offset + ext_offset, it->data_file_offset + ext_offset, it->len - ext_offset);
        int64_t rlen = std::min(static_cast<int64_t>(bsize), fsize - offset);

        ssize_t read;
        auto res = sfs().read_ext(curr_ext, 0, static_cast<size_t>(rlen), buffer, read);

        if(res!=0)
        {
            throw duckdb::IOException("Error reading extent code " + std::to_string(res));
        }
    
        if(multiPartDownloadData)
        {
            XLOGF(DBG0, "Reading off {} len {} of fpath {} total_len {} part {}", offset, read, path, fsize, multiPartDownloadData->currExt.start);
        }
        else
        {
            XLOGF(DBG0, "Reading off {} len {} of fpath {} total_len {}", offset, read, path, fsize);
        }

        offset += read;
        buffer += read;
        bsize -= read;
    }

    pos = offset;

    if(bsize>0)
    {
        throw duckdb::IOException("Could not read " + std::to_string(bsize) + " bytes from file: " + path);
    }
}

int64_t DuckDbFileHandle::Read(char *buffer, int64_t bsize)
{
    const auto toread = (std::min)(bsize, fsize - pos);
    Read(buffer, toread, pos);
    return toread;
}

time_t DuckDbFileHandle::LastModifiedTime()
{
    constexpr auto epoch = std::chrono::time_point<std::chrono::system_clock>();
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(epoch + std::chrono::nanoseconds(lastModified));
    return lastModifiedTp;
}

void DuckDbFs::RemoveDirectory(const std::string &path, duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    ListFiles(
	    path,
	    [&](const std::string &file, bool is_dir) {
		    try {
			    this->RemoveFile(file, opener);
		    } catch (duckdb::IOException &e) {
			    std::string errmsg(e.what());
			    if (errmsg.find("No such file or directory") != std::string::npos) {
				    return;
			    }
			    throw;
		    }
	    },
	    opener.get());
}

bool DuckDbFs::ListFiles(const std::string &directory, const std::function<void(const std::string &, bool)> &callback,
                             duckdb::FileOpener *opener) {
	std::string trimmed_dir = directory;
	duckdb::StringUtil::RTrim(trimmed_dir, PathSeparator(trimmed_dir));
	auto glob_res = Glob(JoinPath(trimmed_dir, "**"), opener);

	if (glob_res.empty()) {
		return false;
	}

	for (const auto &file : glob_res) {
		callback(file.path, false);
	}

	return true;
}

static std::string GetPrefix(std::string url) {
	const std::string prefixes[] = {"hs5://"};
	for (auto &prefix : prefixes) {
		if (duckdb::StringUtil::StartsWith(url, prefix)) {
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
	if (slash_pos == std::string::npos) {
		throw duckdb::IOException("URL needs to contain a '/' after the host");
	}

    const auto bucket = url.substr(prefix_end_pos, slash_pos - prefix_end_pos);

    if (bucket.empty()) {
		throw duckdb::IOException("URL needs to contain a bucket name");
	}

    const auto key = url.substr(slash_pos + 1);

    return {prefix, bucket, key};
}

duckdb::vector<duckdb::OpenFileInfo> DuckDbFs::Glob(const std::string &glob_pattern, duckdb::FileOpener *opener)
{
    const auto parsedUrl = Hs5UrlParse(glob_pattern);

    const auto firstWildcardPos = parsedUrl.key.find_first_of("*[\\");
	if (firstWildcardPos == std::string::npos) {
		return {glob_pattern};
	}

    auto bucketId = buckets::getBucket(parsedUrl.bucket);
    if(!bucketId)
    {
        throw duckdb::IOException("Bucket not found");
    }

    const auto sharedPath = std::string_view(parsedUrl.key).substr(0, firstWildcardPos);
    const auto patternSplits = duckdb::StringUtil::Split(parsedUrl.key, "/");

    const auto iterStartVal = make_key(sharedPath, *bucketId, std::numeric_limits<int64_t>::max());

    sfs.list_commit();

    SingleFileStorage::IterData iterData;

    if(!sfs.iter_start(iterStartVal, false, iterData))
    {
        throw duckdb::IOException("Failed to start listing");
    }

    auto guard = folly::makeGuard([&] { sfs.iter_stop(iterData); });

    std::string lastOutputKeyStr;

    duckdb::vector<duckdb::OpenFileInfo> ret;

    while(true)
    {
        std::string keyBin, md5sum;
        int64_t offset, size, last_modified;
        std::vector<SingleFileStorage::SPunchItem> extra_exts;
        if(!sfs.iter_curr_val(keyBin, offset, size, extra_exts, last_modified, md5sum, iterData))
        {
            break;
        }

        const auto keyInfo = extractKeyInfoView(keyBin);

        if(!keyInfo.key.starts_with(sharedPath))
        {
            break;
        }

        if(keyInfo.bucketId != bucketId)
        {
            break;
        }

        bool outputKey = true;

        if(keyInfo.key == lastOutputKeyStr)
        {
            outputKey = false;
        }
        else if(md5sum.size() == 1 && md5sum[0] == metadata_tombstone)
        {
            lastOutputKeyStr = keyInfo.key;
            outputKey = false;
        }

        if (outputKey)
        {
            lastOutputKeyStr = keyInfo.key;

            const auto keySplits = duckdb::StringUtil::Split(std::string(keyInfo.key), "/");
		    const auto isMatch = Match(keySplits.begin(), keySplits.end(), patternSplits.begin(), patternSplits.end());
            if(isMatch)
            {
                ret.emplace_back(parsedUrl.http_proto + parsedUrl.bucket + "/" + std::string(keyInfo.key));
            }
        }

        if(!sfs.iter_next(iterData))
        {
            throw duckdb::IOException("Failed to list next");
        }
    }

    return ret;
}

void DuckDbFs::Read(duckdb::FileHandle &handle, void *buffer, int64_t nr_bytes, duckdb::idx_t location)
{
    handle.Cast<DuckDbFileHandle>().Read(static_cast<char *>(buffer), nr_bytes, location);
}

int64_t DuckDbFs::Read(duckdb::FileHandle &handle, void *buffer, int64_t nr_bytes)
{
    return handle.Cast<DuckDbFileHandle>().Read(static_cast<char *>(buffer), nr_bytes);
}

bool DuckDbFs::CanHandleFile(const std::string &fpath)
{
    return fpath.starts_with("hs5://");
}

duckdb::unique_ptr<duckdb::FileHandle> DuckDbFs::OpenFile(const std::string &path, duckdb::FileOpenFlags flags,
    duckdb::optional_ptr<duckdb::FileOpener> opener)
{
    if (flags.ReturnNullIfNotExists()) 
    {
        try
        {
            return duckdb::make_uniq<DuckDbFileHandle>(*this, path, flags);
        }
        catch (duckdb::IOException &e)
        {
            if (std::string(e.what()).starts_with("File not found"))
                return nullptr;
            throw;
        }
    }

    return duckdb::make_uniq<DuckDbFileHandle>(*this, path, flags);
}

int64_t DuckDbFs::GetFileSize(duckdb::FileHandle &handle)
{
    return handle.Cast<DuckDbFileHandle>().FileSize();
}

time_t DuckDbFs::GetLastModifiedTime(duckdb::FileHandle &handle)
{
    return handle.Cast<DuckDbFileHandle>().LastModifiedTime();
}