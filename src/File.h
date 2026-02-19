#pragma once
#include <fcntl.h>
#include <folly/File.h>
#include <folly/Exception.h>
#include <folly/FileUtil.h>
#include <folly/Format.h>
#include <mutex>
#include <vector>
#include <folly/logging/xlog.h>
#include <folly/String.h>
#include <shared_mutex>
#include "os_functions.h"

constexpr int64_t fillPageSize = 4096;

char *zeroBuf();

class File : public folly::File
{
public:
    using folly::File::File;

    File(folly::File&& other)
        : folly::File::File(std::move(other))
    {

    }

    int64_t size() const
    { 
        struct stat64 st;
        folly::checkUnixError(fstat64(fd(), &st), "fstat() failed");
        return st.st_size;
    }

    bool punchHole(__off64_t spos, __off64_t size) const
    {
        int rc = fallocate64(fd(), FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, spos, size);
        return rc == 0;
    }
    
    void truncate(__off64_t len) const
    {
        folly::checkUnixError(ftruncate64(fd(), len), "Truncate failed");
    }

    ssize_t preadFull(void* buf, size_t count, off64_t offset) const
    {
        return folly::preadFull(fd(), buf, count, offset);
    }

    ssize_t pwriteFull(const void* buf, size_t count, off64_t offset) const
    {
        return folly::pwriteFull(fd(), buf, count, offset);
    }

    ssize_t pwritevFull(iovec* iov, int count, off64_t offset) const
    {
        return folly::pwritevFull(fd(), iov, count, offset);
    }

    int fsyncNoInt() const
    {
        return folly::fsyncNoInt(fd());
    }

    int fadvise(off64_t offset, off64_t len,
			    int advise) const
    {
        return posix_fadvise64(fd(), offset, len, advise);
    }
};

class MultiFile
{
public:
    MultiFile()
        : maxSize(std::string::npos), flags(O_RDONLY), mode(0666)
    {}

    MultiFile(std::string namePrefixParam, int64_t maxSize, int flags = O_RDONLY, mode_t mode = 0666)
     : namePrefix(std::move(namePrefixParam)), flags(flags), mode(mode), maxSize(maxSize)
    {
        size_t idx=0;
        int64_t lastSize = -1;
        while(true)
        {
            const auto fn = namePrefix + std::to_string(idx);

            if(os_get_file_type(fn)==0)
                break;

            auto file = File(fn, (flags & ~O_CREAT), mode);

            if(idx>0 && lastSize!=maxSize)
            {
                XLOGF(ERR, "While opening multi-file {}{} is {} != expected {}", namePrefix, idx-1, lastSize, maxSize);
                chunks.clear();
                return;
            }

            lastSize = file.size();

            chunks.push_back(std::move(file));
            ++idx;
        }

        if( ((flags& O_RDWR) || (flags & O_WRONLY) )
            && chunks.empty())
        {
            auto file = File(namePrefix + "0", flags | O_CREAT, mode);
            if(!file)
                return;
            chunks.push_back(std::move(file));
        }
    }

    MultiFile(const MultiFile& other) = delete;

    MultiFile(MultiFile&& other) noexcept
        : namePrefix(other.namePrefix), flags(other.flags), mode(other.mode), maxSize(other.maxSize),
        chunks(std::exchange(other.chunks, {}))
    {

    }

    MultiFile& operator=(const MultiFile& other) = delete;

    MultiFile& operator=(MultiFile&& other) noexcept
    {
        namePrefix = other.namePrefix;
        flags = other.flags;
        mode = other.mode;
        maxSize = other.maxSize;
        std::swap(chunks, other.chunks);
        return *this;
    }

    template<typename T>
    ssize_t mapOp(T op, size_t count, int64_t offset, bool extend)
    {
        std::shared_lock readLock(mutex);

         ssize_t ret = 0;
        while(count>0)
        {
            size_t part = static_cast<size_t>(offset / maxSize);
            int64_t partOffset = offset % maxSize;

            if(part>=chunks.size())
            {
                if(!extend)
                    return ret==0 ? -1 : ret;

                readLock.unlock();

                {
                    std::unique_lock lock(mutex);

                    auto nextIdx = chunks.size();
                    while(part>=chunks.size())
                    {
                        auto file = File(namePrefix + std::to_string(nextIdx), flags | O_CREAT, mode);
                        if(!file)
                        {
                            XLOGF(ERR, "Error opening next multifile {}{} errno {}", namePrefix, nextIdx, folly::errnoStr(errno));
                            return ret==0 ? -1 : ret;
                        }

                        XLOGF(DBG0, "Created next multifile {}{}", namePrefix, nextIdx);

                        chunks.push_back(std::move(file));
                        ++nextIdx;
                    }
                }

                readLock.lock();
            }

            auto& file = chunks[part];

            const auto currCount = static_cast<size_t>((std::min)(static_cast<int64_t>(count), maxSize - partOffset));

            int rc = op(file, currCount, partOffset);

            if(rc<=0)
                return ret==0 ? rc : ret;

            count-=rc;
            offset+=rc;
            ret+=rc;
        }

        return ret;
    }

    explicit operator bool() const
    { 
        std::shared_lock readLock(mutex);

        return !chunks.empty();
    }

    ssize_t preadFull(char* buf, size_t count, off64_t offset)
    {
        return mapOp([&](const File& file, size_t count, size_t offset) {
            const auto rc = file.preadFull(buf, count, offset);
            if(rc>0)
                buf+=rc;
            return rc;
        }, count, offset, false);
    }

    ssize_t pwriteFull(const char* buf, size_t count, off64_t offset)
    {
        return mapOp([&](const File& file, size_t count, size_t offset) {
            const auto rc = file.pwriteFull(buf, count, offset);
            if(rc>0)
                buf+=rc;
            return rc;
        }, count, offset, true);
    }

    ssize_t pwriteFullFillPage(const char* buf, size_t count, off64_t offset)
    {
        return mapOp([&](const File& file, size_t mapCount, size_t offset) {
            
            if(mapCount==count && 
                offset % fillPageSize == 0 &&
                mapCount % fillPageSize != 0)
            {
                iovec iov[2];
                iov[0].iov_base = const_cast<char*>(buf);
                iov[0].iov_len = mapCount;
                iov[1].iov_base = zeroBuf();
                iov[1].iov_len = fillPageSize - (mapCount % fillPageSize);
                const auto rc = file.pwritevFull(iov, 2, offset);
                if(rc==-1)
                    return rc;

                buf+=mapCount;
                return static_cast<ssize_t>(mapCount);
            }

            const auto rc = file.pwriteFull(buf, mapCount, offset);
            if(rc>0)
                buf+=rc;
            return rc;
        }, count, offset, true);
    }

    int64_t size() const
    {
        std::shared_lock readLock(mutex);

        if(chunks.empty())
            return 0;

        const auto& lastFile = chunks.back();
        return lastFile.size() + (chunks.size()-1)*maxSize;
    }

    int fsyncNoInt() const
    {
        std::shared_lock readLock(mutex);

        int rc = 0;
        for(const auto& file: chunks)
        {
            auto currRc = file.fsyncNoInt();
            if(currRc!=0)
                rc = currRc;
        }
        return rc;
    }

    bool punchHole(__off64_t spos, __off64_t size)
    {
        return mapOp([](const File& file, size_t count, size_t offset) {
            return file.punchHole(offset, count) ? count : -1;
        }, size, spos, false) == 0;
    }

    
    bool alloc(__off64_t len)
    {
        if(len<=0)
            return false;

        return mapOp([&](const File& file, size_t count, size_t offset) {
            return posix_fallocate(file.fd(), 0, offset+count) == 0 ? 1 : -1;
        }, 1, len-1, true) == 1;
    }

    void truncate(__off64_t len)
    {
        const auto csize = size();

        if(len>csize)
        {
            mapOp([](const File& file, size_t count, size_t offset) {
                file.truncate(offset+count);
                return count;
            }, len - csize, csize, true);
        }
        else
        {
            std::unique_lock lock(mutex);

            auto toTruncate = csize - len;

            while(toTruncate>0)
            {
                if(chunks.empty())
                    return;

                auto& lastChunk = chunks.back();

                const auto chunkSize = lastChunk.size();

                const auto currTrunc = std::min(chunkSize, toTruncate);

                if(currTrunc==chunkSize)
                {
                    std::string fn = namePrefix + std::to_string(chunks.size()-1);
                    folly::checkUnixError(unlink(fn.c_str()), fmt::format("Error unlinking {} during truncation", fn));
                    chunks.pop_back();
                }
                else
                {
                    lastChunk.truncate(chunkSize-currTrunc);
                }

                toTruncate -= currTrunc;
            }
        }                
    }

private:

    std::string namePrefix;
    int flags;
    mode_t mode;
    int64_t maxSize;
    std::vector<File> chunks;
    mutable std::shared_mutex mutex;
};