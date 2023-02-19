/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include "os_functions.h"
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/ioctl.h>
#include <limits.h>

int64_t os_total_space(const std::string &path)
{
	std::string cp=path;
	if(path.size()==0)
		return -1;
	if(cp[cp.size()-1]=='/')
		cp.erase(cp.size()-1, 1);
	if(cp[cp.size()-1]!='/')
		cp+='/';

	struct statvfs64 buf;
    int rc=statvfs64((path).c_str(), &buf);
	if(rc==0)
	{
		fsblkcnt64_t used=buf.f_blocks-buf.f_bfree;
#if defined(__FreeBSD__) || defined(__APPLE__)
		int64 total = (int64)(used+buf.f_bavail)*buf.f_frsize;
#else
		fsblkcnt64_t total = (used+buf.f_bavail)*buf.f_bsize;
#endif
		if(total>LLONG_MAX)
		{
			return LLONG_MAX;
		}
		return total;
	}
	else
		return -1;
}

int64_t os_free_space(const std::string &path)
{
	std::string cp=path;
	if(path.size()==0)
		return -1;
	if(cp[cp.size()-1]=='/')
		cp.erase(cp.size()-1, 1);
	if(cp[cp.size()-1]!='/')
		cp+='/';

	struct statvfs64 buf = {};
    int rc=statvfs64((path).c_str(), &buf);
	if(rc==0)
	{
#if defined(__FreeBSD__) || defined(__APPLE__)
		int64 free = (int64)buf.f_frsize*buf.f_bavail;
#else
		fsblkcnt64_t blocksize = buf.f_frsize ? buf.f_frsize : buf.f_bsize;
		fsblkcnt64_t free = blocksize*buf.f_bavail;
#endif
		if(free>LLONG_MAX)
		{
			return LLONG_MAX;
		}
		return free;
	}
	else
	{
		return -1;
	}
}

unsigned int os_get_file_type(const std::string &path)
{
	int ret = 0;
	struct stat64 f_info;
    int rc1=stat64((path).c_str(), &f_info);
	if(rc1==0)
	{
		if ( S_ISDIR(f_info.st_mode) )
        {
			ret |= EFileType_Directory;
		}
		else
		{
			ret |= EFileType_File;
		}
	}

    int rc2 = lstat64((path).c_str(), &f_info);
	if(rc2==0)
	{
		if(S_ISLNK(f_info.st_mode))
		{
			ret |= EFileType_Symlink;
		}
		
		if(!S_ISDIR(f_info.st_mode)
			&& !S_ISREG(f_info.st_mode) )
		{
			ret |= EFileType_Special;
		}
		
		if(rc1!=0)
		{
			ret |= EFileType_File;
		}
	}
	
	return ret;
}

int os_popen(const std::string& cmd, std::string& ret)
{
	ret.clear();

#ifdef __ANDROID__
    POFILE* pin = NULL;
#endif

	FILE* in = NULL;

#ifndef _WIN32
#define _popen popen
#define _pclose pclose
#endif

#ifdef __ANDROID__
    pin = and_popen(cmd.c_str(), "r");
    if(pin!=NULL) in=pin->fp;
#elif __linux__
	in = _popen(cmd.c_str(), "re");
	if(!in) in = _popen(cmd.c_str(), "r");
#else
	in = _popen(cmd.c_str(), "r");
#endif

	if(in==NULL)
	{
		return -1;
	}

	char buf[4096];
	size_t read;
	do
	{
		read=fread(buf, 1, sizeof(buf), in);
		if(read>0)
		{
			ret.append(buf, buf+read);
		}
	}
	while(read==sizeof(buf));

#ifdef __ANDROID__
    return and_pclose(pin);
#else
    return _pclose(in);
#endif
}

#define BTRFS_IOCTL_MAGIC 0x94
#define BTRFS_IOC_SYNC _IO(BTRFS_IOCTL_MAGIC, 8)

bool os_sync(const std::string & path)
{
#if defined(__linux__)
	int fd = open(path.c_str(), O_RDONLY|O_CLOEXEC);
	
	if(fd!=-1)
	{
		if(ioctl(fd, BTRFS_IOC_SYNC, NULL)==-1)
		{
			if(errno!=ENOTTY && errno!=ENOSYS 
				&& errno!=EINVAL)
			{
				close(fd);
				return false;
			}
		}
		else
		{
			close(fd);
			return true;
		}

		if(syncfs(fd)!=0)
		{
			if(errno==ENOSYS)
			{
				close(fd);
				sync();
				return true;
			}
			close(fd);
			return false;
		}
		else
		{
			close(fd);
			return true;
		}
	}
	else
	{
		sync();
		return true;
	}
#else
	sync();
	return true;
#endif
}