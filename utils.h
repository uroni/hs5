#include <string>
#include <stdint.h>


static bool next(const std::string &pData, const size_t & doff, const std::string &pStr)
{
        for(size_t i=0;i<pStr.size();++i)
        {
                if( i+doff>=pData.size() )
                        return false;
                if( pData[doff+i]!=pStr[i] )
                        return false;
        }
        return true;
}

static std::string getafter(const std::string &str,const std::string &data)
{
    size_t pos=data.find(str);
	if(pos!=std::string::npos)
	{
		return data.substr(pos + str.size());
	}
	else
	{
		return std::string();
	}
}

static std::string getuntil(const std::string& str, const std::string& data)
{
	size_t off=data.find(str);
	if(off==std::string::npos)
		return std::string();
	return data.substr(0,off);
}

static bool isHex(const std::string &str)
{
	if (str.size() % 2 != 0)
	{
		return false;
	}

	for(size_t i=0;i<str.size();++i)
	{
		char ch = str[i];
		if (ch >= '0'
			&& ch <= '9')
		{
		}
		else if (ch >= 'a' && ch <= 'f')
		{			
		}
		else if (ch >= 'A' && ch <= 'F')
		{			
		}
		else
		{
			return false;
		}
	}
	return true;
}

std::string random_uuid();

std::string random_uuid_binary();

std::string escapeXML(const std::string & xml);