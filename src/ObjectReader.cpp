/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */
#include "ObjectReader.h"
#include "Buckets.h"
#include "s3handler.h"

ObjectReader::ObjectReader(SingleFileStorage& sfs, const std::string& bucket, const std::string& key)
    : sfs(sfs), name(bucket + "/" + key)
{
    const auto bucketInfoOpt = buckets::getBucketInfo(bucket);
    if (!bucketInfoOpt)
    {
        throw ObjectReadError("Bucket not found");
    }

    const auto& bucketInfo = bucketInfoOpt.value();

    bucketId = bucketInfo.id;

    unsigned int readPrepFlags = SingleFileStorage::ReadAddReadingCallback;

    const auto withVersioning = bucketInfo.versioning == buckets::VersioningState::Enabled || bucketInfo.versioning == buckets::VersioningState::Suspended;
    if (withVersioning)
    {
        s3key = make_key(key, bucketId, std::numeric_limits<int64_t>::max());
        readPrepFlags |= SingleFileStorage::ReadNewest;
    }
    else
    {
        s3key = make_key(key, bucketId, 0);
    }

    auto res = sfs.read_prepare(s3key, readPrepFlags);

    if (res.err != 0)
    {
        if (res.err == ENOENT)
        {
            throw ObjectReadError("File not found: " + name, true);
        }
        throw ObjectReadError("Failed to prepare read: " + std::to_string(res.err));
    }

    if (withVersioning)
        s3key = res.key;

    if (!S3Handler::parseMultipartInfo(res.md5sum, res.total_len, multiPartDownloadData, &objMetadata, &sdkChecksumHash))
    {
        throw ObjectReadError("Failed to parse multipart info for file: " + name);
    }

    if (res.md5sum.size() == 1 && res.md5sum[0] == metadata_tombstone)
    {
        throw ObjectReadError("File not found: " + name, true);
    }

    extents = std::move(res.extents);
    open = true;
    fsize = res.total_len;
    lastModified = res.last_modified;

    if (!multiPartDownloadData)
        etag = S3Handler::md5sumBinFromData(res.md5sum);
}

ObjectReader::~ObjectReader()
{
    Close();
}

void ObjectReader::Close()
{
    if (!open)
        return;

    if (multiPartDownloadData)
    {
        const auto rc = S3Handler::finalizeMultiPart(sfs, s3key, bucketId, *multiPartDownloadData, extents);
        if (rc)
            XLOGF(WARN, "Error finalizing multipart download: {}", rc);
    }
    else
    {
        sfs.read_finalize(s3key, extents, 0);
    }

    open = false;
}

void ObjectReader::Read(char* buffer, int64_t bsize, int64_t offset)
{
    if (multiPartDownloadData)
    {
        const auto rc = S3Handler::seekMultipartExt(sfs, offset, bucketId, *multiPartDownloadData, extents);
        if (rc)
        {
            throw ObjectReadError("Error seeking to part rc " + std::to_string(rc));
        }
    }

    while (offset < fsize && bsize > 0)
    {
        auto it = std::upper_bound(extents.begin(), extents.end(), SingleFileStorage::Ext(offset, 0, 0));
        if (!extents.empty())
            --it;
        assert(it != extents.end());
        if (it == extents.end() || it->len == 0 || it->obj_offset + it->len <= offset)
        {
            if (multiPartDownloadData)
            {
                const int rc = S3Handler::readNextMultipartExt(sfs, offset, bucketId, true, *multiPartDownloadData, extents, nullptr, nullptr, nullptr);
                if (rc)
                {
                    throw ObjectReadError("Error reading next part code " + std::to_string(rc));
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
        auto res = sfs.read_ext(curr_ext, 0, static_cast<size_t>(rlen), buffer, read);

        if (res != 0)
        {
            throw ObjectReadError("Error reading extent code " + std::to_string(res));
        }

        if (multiPartDownloadData)
        {
            XLOGF(DBG0, "Reading off {} len {} of {} total_len {} part {}", offset, read, name, fsize, multiPartDownloadData->currExt.start);
        }
        else
        {
            XLOGF(DBG0, "Reading off {} len {} of {} total_len {}", offset, read, name, fsize);
        }

        offset += read;
        buffer += read;
        bsize -= read;
    }

    pos = offset;

    if (bsize > 0)
    {
        throw ObjectReadError("Could not read " + std::to_string(bsize) + " bytes from file: " + name);
    }
}

int64_t ObjectReader::Read(char* buffer, int64_t bsize)
{
    const auto toread = (std::min)(bsize, fsize - pos);
    Read(buffer, toread, pos);
    return toread;
}

time_t ObjectReader::LastModifiedTime() const
{
    constexpr auto epoch = std::chrono::time_point<std::chrono::system_clock>();
    const auto lastModifiedTp = std::chrono::system_clock::to_time_t(epoch + std::chrono::nanoseconds(lastModified));
    return lastModifiedTp;
}

std::string ObjectReader::ETag() const
{
    if (multiPartDownloadData)
        return fmt::format("\"{}-{}\"", folly::hexlify(multiPartDownloadData->etag), multiPartDownloadData->numParts);
    else
        return fmt::format("\"{}\"", folly::hexlify(etag));
}
