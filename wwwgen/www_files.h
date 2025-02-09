#pragma once
#include <map>
#include <string>

extern unsigned char index_html[];
extern unsigned int index_html_len;

extern unsigned char favicon_ico[];
extern unsigned int favicon_ico_len;

extern unsigned char index_4363833e_js[];
extern unsigned int index_4363833e_js_len;

struct WwwFile
{
	const unsigned char* data;
	unsigned int len;
	std::string etag;
	bool immutable;
	std::string_view contentType;
	unsigned int uncompLen;
};
static std::map<std::string_view, WwwFile> www_files = {
  { "index.html", WwwFile{ index_html, index_html_len, "\"d41d8cd98f00b204e9800998ecf8427e\"", false, "text/html", 0 } },
  { "favicon.ico", WwwFile{ favicon_ico, favicon_ico_len, "\"d41d8cd98f00b204e9800998ecf8427e\"", false, "image/x-icon", 0 } },
  { "admin-b64be5124b034028a58913931942e205/js/index-4363833e.js", WwwFile{ index_4363833e_js, index_4363833e_js_len, "\"d41d8cd98f00b204e9800998ecf8427e\"", false, "application/javascript", 0 } },
};
