#pragma once

#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <folly/File.h>
#include <folly/Exception.h>
#include <stdint.h>

static std::string os_file_sep() {
    return "/";
}

static pid_t getThreadID() {
	return syscall(__NR_gettid);
}

static int64_t fileSize(int fd)
{
    struct stat64 st;
    folly::checkUnixError(fstat64(fd, &st), "fstat() failed");
    return st.st_size;
}

int64_t os_total_space(const std::string& path);

int64_t os_free_space(const std::string& path);

enum EFileType
{
	EFileType_File = 1,
	EFileType_Directory = 2,
	EFileType_Symlink = 4,
	EFileType_Special = 8
};

unsigned int os_get_file_type(const std::string& path);

int os_popen(const std::string& cmd, std::string& ret);

bool os_sync(const std::string & path);