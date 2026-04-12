#pragma once

#include <string>
#include <stdint.h>
#include <folly/String.h>

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

std::string escapeXML(const std::string_view xml);

std::string rfc2047EncodeB(std::string_view text, std::string_view charset = "UTF-8");

std::string rfc2047Decode(const std::string_view text);

std::string asciiToLower(std::string str);

// Escape using upper case hex values. Copied & adjusted folly::uriEscape() 
template <class String>
void uriEscapeUpper(folly::StringPiece str, String& out, folly::UriEscapeMode mode) {
  static const char hexValues[] = "0123456789ABCDEF";
  char esc[3];
  esc[0] = '%';
  // Preallocate assuming that 25% of the input string will be escaped
  folly::grow_capacity_by(out, str.size() + 3 * (str.size() / 4));
  auto p = str.begin();
  auto last = p; // last regular character
  // We advance over runs of passthrough characters and copy them in one go;
  // this is faster than calling push_back repeatedly.
  unsigned char minEncode = static_cast<unsigned char>(mode);
  while (p != str.end()) {
    char c = *p;
    unsigned char v = static_cast<unsigned char>(c);
    unsigned char discriminator = folly::detail::uriEscapeTable[v];
    if (FOLLY_LIKELY(discriminator <= minEncode)) {
      ++p;
    } else if (mode == folly::UriEscapeMode::QUERY && discriminator == 3) {
      out.append(&*last, size_t(p - last));
      out.push_back('+');
      ++p;
      last = p;
    } else {
      out.append(&*last, size_t(p - last));
      esc[1] = hexValues[v >> 4];
      esc[2] = hexValues[v & 0x0f];
      out.append(esc, 3);
      ++p;
      last = p;
    }
  }
  out.append(&*last, size_t(p - last));
}

template <class String>
String uriEscapeUpper(folly::StringPiece str, folly::UriEscapeMode mode = folly::UriEscapeMode::ALL) {
  String out;
  uriEscapeUpper(str, out, mode);
  return out;
}