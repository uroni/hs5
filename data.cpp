/**
 * Copyright Martin Raiber. All Rights Reserved.
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include <memory.h>
#include "data.h"
#include <assert.h>
#include <stdint.h>

namespace
{
bool is_big_endian(void)
{
    union {
        unsigned int i;
        char c[4];
    } bint = {0x01020304};

    return bint.c[0] == 1; 
}

unsigned int endian_swap(unsigned int x)
{
    return (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}

unsigned short endian_swap(unsigned short x)
{
    return x = (x>>8) | 
        (x<<8);
}

std::string endian_swap_utf16(std::string str)
{
	for(size_t i=0;i<str.size();i+=2)
	{
		unsigned short *t=(unsigned short*)&str[i];
		*t=endian_swap(*t);
	}
	return str;
}

uint64_t endian_swap(uint64_t x)
{
#ifdef _WIN32
    return (x>>56) | 
        ((x<<40) & 0x00FF000000000000) |
        ((x<<24) & 0x0000FF0000000000) |
        ((x<<8)  & 0x000000FF00000000) |
        ((x>>8)  & 0x00000000FF000000) |
        ((x>>24) & 0x0000000000FF0000) |
        ((x>>40) & 0x000000000000FF00) |
        (x<<56);
#else
    return (x>>56) | 
        ((x<<40) & 0x00FF000000000000LLU) |
        ((x<<24) & 0x0000FF0000000000LLU) |
        ((x<<8)  & 0x000000FF00000000LLU) |
        ((x>>8)  & 0x00000000FF000000LLU) |
        ((x>>24) & 0x0000000000FF0000LLU) |
        ((x>>40) & 0x000000000000FF00LLU) |
        (x<<56);
#endif
}

unsigned int little_endian(unsigned int x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

unsigned short little_endian(unsigned short x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int little_endian(int x)
{
	if(is_big_endian())
	{
		return static_cast<int>(endian_swap(static_cast<unsigned int>(x)));
	}
	else
	{
		return x;
	}
}

uint64_t little_endian(uint64_t x)
{
	if(is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int64_t little_endian(int64_t x)
{
	if(is_big_endian())
	{
		return static_cast<int64_t>(endian_swap(static_cast<uint64_t>(x)));
	}
	else
	{
		return x;
	}
}

float little_endian(float x)
{
	if(is_big_endian())
	{
		unsigned int* ptr=reinterpret_cast<unsigned int*>(&x);
		unsigned int ret = endian_swap(*ptr);
		return *reinterpret_cast<float*>(&ret);
	}
	else
	{
		return x;
	}
}

double little_endian(double x)
{
	if (is_big_endian())
	{
		uint64_t* ptr = reinterpret_cast<uint64_t*>(&x);
		uint64_t ret = endian_swap(*ptr);
		return *reinterpret_cast<double*>(&ret);
	}
	else
	{
		return x;
	}
}

unsigned int big_endian(unsigned int x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

unsigned short big_endian(unsigned short x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int big_endian(int x)
{
	if(!is_big_endian())
	{
		return static_cast<int>(endian_swap(static_cast<unsigned int>(x)));
	}
	else
	{
		return x;
	}
}

uint64_t big_endian(uint64_t x)
{
	if(!is_big_endian())
	{
		return endian_swap(x);
	}
	else
	{
		return x;
	}
}

int64_t big_endian(int64_t x)
{
	if(!is_big_endian())
	{
		return static_cast<int64_t>(endian_swap(static_cast<uint64_t>(x)));
	}
	else
	{
		return x;
	}
}

float big_endian(float x)
{
	if(!is_big_endian())
	{
		unsigned int* ptr=reinterpret_cast<unsigned int*>(&x);
		unsigned int ret = endian_swap(*ptr);
		return *reinterpret_cast<float*>(&ret);
	}
	else
	{
		return x;
	}
}

std::string big_endian_utf16(std::string str)
{
	if(!is_big_endian())
	{
		return endian_swap_utf16(str);
	}
	else
	{
		return str;
	}
}

	//FROM SQLITE

	typedef uint64_t u64;
	typedef unsigned char u8;
	typedef uint32_t u32;

	/*
	** Return the number of bytes that will be needed to store the given
	** 64-bit integer.
	*/
	int sqlite3VarintLen(u64 v){
		int i;
		for(i=1; (v >>= 7)!=0; i++)
		{
			assert( i<10 );
		}

		if (i == 10)
		{
			return 9;
		}

		return i;
	}

	/*
	** Bitmasks used by sqlite3GetVarint().  These precomputed constants
	** are defined here rather than simply putting the constant expressions
	** inline in order to work around bugs in the RVT compiler.
	**
	** SLOT_2_0     A mask for  (0x7f<<14) | 0x7f
	**
	** SLOT_4_2_0   A mask for  (0x7f<<28) | SLOT_2_0
	*/
#define SLOT_2_0     0x001fc07f
#define SLOT_4_2_0   0xf01fc07f


	/*
	** Read a 64-bit variable-length integer from memory starting at p[0].
	** Return the number of bytes read.  The value is stored in *v.
	*/
	u8 sqlite3GetVarint(const unsigned char *p, u64 *v, size_t max_length){
		u32 a,b,s;

		if(max_length==0)
		{
			return 0;
		}

		a = *p;
		/* a: p0 (unmasked) */
		if (!(a&0x80))
		{
			*v = a;
			return 1;
		}

		if(max_length<=1)
		{
			return 0;
		}

		p++;
		b = *p;
		/* b: p1 (unmasked) */
		if (!(b&0x80))
		{
			a &= 0x7f;
			a = a<<7;
			a |= b;
			*v = a;
			return 2;
		}

		/* Verify that constants are precomputed correctly */
		assert( SLOT_2_0 == ((0x7f<<14) | (0x7f)) );
		assert( SLOT_4_2_0 == ((0xfU<<28) | (0x7f<<14) | (0x7f)) );

		if(max_length<=2)
		{
			return 0;
		}

		p++;
		a = a<<14;
		a |= *p;
		/* a: p0<<14 | p2 (unmasked) */
		if (!(a&0x80))
		{
			a &= SLOT_2_0;
			b &= 0x7f;
			b = b<<7;
			a |= b;
			*v = a;
			return 3;
		}

		/* CSE1 from below */
		a &= SLOT_2_0;

		if(max_length<=3)
		{
			return 0;
		}

		p++;
		b = b<<14;
		b |= *p;
		/* b: p1<<14 | p3 (unmasked) */
		if (!(b&0x80))
		{
			b &= SLOT_2_0;
			/* moved CSE1 up */
			/* a &= (0x7f<<14)|(0x7f); */
			a = a<<7;
			a |= b;
			*v = a;
			return 4;
		}

		/* a: p0<<14 | p2 (masked) */
		/* b: p1<<14 | p3 (unmasked) */
		/* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
		/* moved CSE1 up */
		/* a &= (0x7f<<14)|(0x7f); */
		b &= SLOT_2_0;
		s = a;
		/* s: p0<<14 | p2 (masked) */

		if(max_length<=4)
		{
			return 0;
		}

		p++;
		a = a<<14;
		a |= *p;
		/* a: p0<<28 | p2<<14 | p4 (unmasked) */
		if (!(a&0x80))
		{
			/* we can skip these cause they were (effectively) done above in calc'ing s */
			/* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
			/* b &= (0x7f<<14)|(0x7f); */
			b = b<<7;
			a |= b;
			s = s>>18;
			*v = ((u64)s)<<32 | a;
			return 5;
		}

		/* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
		s = s<<7;
		s |= b;
		/* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

		if(max_length<=5)
		{
			return 0;
		}

		p++;
		b = b<<14;
		b |= *p;
		/* b: p1<<28 | p3<<14 | p5 (unmasked) */
		if (!(b&0x80))
		{
			/* we can skip this cause it was (effectively) done above in calc'ing s */
			/* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
			a &= SLOT_2_0;
			a = a<<7;
			a |= b;
			s = s>>18;
			*v = ((u64)s)<<32 | a;
			return 6;
		}

		if(max_length<=6)
		{
			return 0;
		}

		p++;
		a = a<<14;
		a |= *p;
		/* a: p2<<28 | p4<<14 | p6 (unmasked) */
		if (!(a&0x80))
		{
			a &= SLOT_4_2_0;
			b &= SLOT_2_0;
			b = b<<7;
			a |= b;
			s = s>>11;
			*v = ((u64)s)<<32 | a;
			return 7;
		}

		/* CSE2 from below */
		a &= SLOT_2_0;

		if(max_length<=7)
		{
			return 0;
		}

		p++;
		b = b<<14;
		b |= *p;
		/* b: p3<<28 | p5<<14 | p7 (unmasked) */
		if (!(b&0x80))
		{
			b &= SLOT_4_2_0;
			/* moved CSE2 up */
			/* a &= (0x7f<<14)|(0x7f); */
			a = a<<7;
			a |= b;
			s = s>>4;
			*v = ((u64)s)<<32 | a;
			return 8;
		}

		if(max_length<=8)
		{
			return 0;
		}

		p++;
		a = a<<15;
		a |= *p;
		/* a: p4<<29 | p6<<15 | p8 (unmasked) */

		/* moved CSE2 up */
		/* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
		b &= SLOT_2_0;
		b = b<<8;
		a |= b;

		s = s<<4;
		b = p[-4];
		b &= 0x7f;
		b = b>>3;
		s |= b;

		*v = ((u64)s)<<32 | a;

		return 9;
	}

	/*
	** Write a 64-bit variable-length integer to memory starting at p[0].
	** The length of data write will be between 1 and 9 bytes.  The number
	** of bytes written is returned.
	**
	** A variable-length integer consists of the lower 7 bits of each byte
	** for all bytes that have the 8th bit set and one byte with the 8th
	** bit clear.  Except, if we get to the 9th byte, it stores the full
	** 8 bits and is the last byte.
	*/
	static int putVarint64(unsigned char *p, u64 v){
		int i, j, n;
		u8 buf[10];
		if( v & (((u64)0xff000000)<<32) ){
			p[8] = (u8)v;
			v >>= 8;
			for(i=7; i>=0; i--){
				p[i] = (u8)((v & 0x7f) | 0x80);
				v >>= 7;
			}
			return 9;
		}    
		n = 0;
		do{
			buf[n++] = (u8)((v & 0x7f) | 0x80);
			v >>= 7;
		}while( v!=0 );
		buf[0] &= 0x7f;
		assert( n<=9 );
		for(i=0, j=n-1; j>=0; j--, i++){
			p[i] = buf[j];
		}
		return n;
	}

	int sqlite3PutVarint(unsigned char *p, u64 v){
		if( v<=0x7f ){
			p[0] = v&0x7f;
			return 1;
		}
		if( v<=0x3fff ){
			p[0] = ((v>>7)&0x7f)|0x80;
			p[1] = v&0x7f;
			return 2;
		}
		return putVarint64(p,v);
	}
}


char* CWData::getDataPtr(void)
{
	if(data.size()>0)
		return &data[0];
	else
		return NULL;
}

unsigned long CWData::getDataSize(void)
{
	return (unsigned long)data.size();
}

void CWData::addInt(int ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta)+sizeof(int));
}

void CWData::addUInt(unsigned int ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(unsigned int));
}

void CWData::addInt64(int64_t ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(int64_t));
}

void CWData::addUInt64(uint64_t ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(uint64_t));
}

void CWData::addFloat(float ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(float));
}

void CWData::addDouble(double ta)
{
	ta = little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(double));
}

void CWData::addUShort(unsigned short ta)
{
	ta=little_endian(ta);
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(unsigned short));
}	

void CWData::addString(const std::string& ta)
{
	addUInt(static_cast<unsigned int>(ta.size()));
	if(!ta.empty())
	{
		data.insert(data.end(), ta.begin(), ta.end());
	}
}

void CWData::addString2(const std::string& ta)
{
	addVarInt(ta.size());
	if (!ta.empty())
	{
		data.insert(data.end(), ta.begin(), ta.end());
	}
}

void CWData::addChar(char ta)
{
	data.insert(data.end(), ta);
}

void CWData::addUChar(unsigned char ta)
{
	data.insert(data.end(), static_cast<char>(ta));
}

void CWData::addVoidPtr(void* ta)
{
	data.insert(data.end(), reinterpret_cast<char*>(&ta), reinterpret_cast<char*>(&ta) + sizeof(void*));
}

void CWData::addBuffer(const char* buffer, size_t bsize)
{
	data.insert(data.end(), buffer, buffer + bsize);
}

void CWData::clear()
{
	data.clear();
}

void CWData::reserve(size_t count)
{
	data.reserve(count);
}

void CWData::resize(size_t count)
{
	data.resize(count);
}

size_t CWData::capacity()
{
	return data.capacity();
}

void CWData::addVarInt( int64_t ta )
{
	size_t cpos=data.size();
	int needed_bytes = sqlite3VarintLen(static_cast<u64>(ta));
	data.resize(cpos+needed_bytes);
	int p = sqlite3PutVarint(reinterpret_cast<unsigned char*>(&data[cpos]), ta);
	assert(p==needed_bytes);
}

CRData::CRData(const char* c,size_t datalength, bool pCopy)
{
	data=NULL;
	set(c,datalength, pCopy);
}

CRData::CRData(void)
{
	data=NULL;
	streampos=0;
	datalen=0;
}

void CRData::set(const char* c,size_t datalength, bool pCopy)
{
	datalen = (std::min)(size_t(100*1024*1024), datalength);

	copy=pCopy;
	if( copy==false )
	{
		data=c;
	}
	else
	{
		if( data!=NULL )
			delete [] data;
		data=new char[datalen];
		memcpy(const_cast<char*>(data), c, datalen);
	}
	streampos=0;
}

CRData::CRData(const std::string *str)
{
	set(str->c_str(), str->size(), false);
}

CRData::~CRData()
{
	if( copy )
		delete []data;
}

bool CRData::getInt(int *ret)
{
	if(streampos+sizeof(int)>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(int) );
	streampos+=sizeof(int);
	*ret=little_endian(*ret);
	return true;
}

bool CRData::getInt64(int64_t *ret)
{
	if(streampos+sizeof(int64_t)>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(int64_t) );
	streampos+=sizeof(int64_t);
	*ret=little_endian(*ret);
	return true;
}

bool CRData::getUInt(unsigned int *ret)
{
	if(streampos+sizeof(unsigned int )>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(unsigned int ) );
	streampos+=sizeof(unsigned int);
	*ret=little_endian(*ret);
	return true;
}
	
bool CRData::getFloat(float *ret)
{
	if(streampos+sizeof(float)>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(float) );
	streampos+=sizeof(float);
	*ret=little_endian(*ret);
	return true;
}

bool CRData::getDouble(double * ret)
{
	if (streampos + sizeof(double)>datalen)
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(double));
	streampos += sizeof(double);
	*ret = little_endian(*ret);
	return true;
}

bool CRData::getUShort( unsigned short *ret)
{
	if(streampos+sizeof(unsigned short)>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(unsigned short) );
	streampos+=sizeof(unsigned short);
	*ret=little_endian(*ret);
	return true;	
}

bool CRData::getStr(std::string *ret)
{
	unsigned int strlen;
	if (!getUInt(&strlen))
	{
		return false;
	}

	if(strlen>10*1024*1024)
	{
		return false;
	}

	if(streampos+strlen>datalen)
	{
		return false;
	}

	if(strlen>0)
	{
		ret->assign(&data[streampos], strlen);
	}
	else
	{
		ret->clear();
	}
	streampos+=strlen;
	return true;
}

bool CRData::getStr2(std::string *ret)
{
	int64_t strlen;
	if (!getVarInt(&strlen))
	{
		return false;
	}

	if (strlen>10 * 1024 * 1024
		|| strlen<0)
	{
		return false;
	}

	if (streampos + strlen>datalen)
	{
		return false;
	}

	if (strlen>0)
	{
		ret->assign(&data[streampos], strlen);
	}
	else
	{
		ret->clear();
	}
	streampos += strlen;
	return true;
}

bool CRData::getChar(char *ret)
{
	if(streampos+sizeof(char)>datalen )
	{
		return false;
	}

	(*ret)=data[streampos];
	streampos+=sizeof(char);

	return true;
}

bool CRData::getUChar(unsigned char *ret)
{
	if(streampos+sizeof(unsigned char)>datalen )
	{
		return false;
	}

	(*ret)=data[streampos];
	streampos+=sizeof(unsigned char);

	return true;
}

bool CRData::getVoidPtr(void **ret)
{
	if(streampos+sizeof(void*)>datalen )
	{
		return false;
	}

	memcpy(ret, &data[streampos], sizeof(void*) );
	streampos+=sizeof(void*);
	return true;
}

bool CRData::getVarInt( int64_t* ret )
{
	u8 b = sqlite3GetVarint(reinterpret_cast<const unsigned char*>(&data[streampos]), reinterpret_cast<u64*>(ret), getLeft());
	streampos+=b;
	return b!=0;
}

unsigned int CRData::getSize(void)
{
	return static_cast<unsigned int>(datalen);
}

unsigned int CRData::getLeft(void)
{
	return static_cast<unsigned int>(datalen - streampos);
}

unsigned int CRData::getStreampos(void)
{
	return static_cast<unsigned int>(streampos);
}

const char *CRData::getDataPtr(void)
{
	return data;
}

const char *CRData::getCurrDataPtr(void)
{
	return data+streampos;
}

void CRData::setStreampos(unsigned int spos)
{
	if( spos <= datalen )
	{
		streampos=spos;
	}
}

bool CRData::incrementPtr(unsigned int amount)
{
	if(amount>getLeft())
		return false;

	streampos+=amount;
	return true;
}


