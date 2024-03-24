#ifndef DATA_H_
#define DATA_H_

#include <string>
#include <vector>
#include <stdint.h>


class CWData
{
public:
	char* getDataPtr(void);
	unsigned long getDataSize(void);

	void addInt(int ta);
	void addUInt(unsigned int ta);
	void addInt64(int64_t ta);
	void addUInt64(uint64_t ta);
	void addFloat(float ta);
	void addDouble(double ta);
	void addUShort(unsigned short ta);
	void addString(const std::string& ta);
	void addString2(const std::string& ta);
	void addChar(char ta);
	void addUChar(unsigned char ta);
	void addVoidPtr(void *ptr);
	void addBuffer(const char* buffer, size_t bsize);
	void addVarInt(int64_t ta);

	void clear();

	void reserve(size_t count);
	void resize(size_t count);

	size_t capacity();

	std::string releaseData()
	{
		return std::move(data);
	}
protected:
	std::string data;
};

class CRData
{
public:
	CRData(const char* c,size_t datalength, bool pCopy=false);
	CRData(const std::string *str);
	CRData(void);
	~CRData();
	
	void set(const char* c,size_t datalength, bool pCopy=false);

	bool getInt(int *ret);
	bool getInt64(int64_t *ret);
	bool getUInt(unsigned int *ret);
	bool getFloat(float *ret);
	bool getDouble(double *ret);
	bool getUShort( unsigned short *ret);
	bool getStr(std::string *ret);
	bool getStr2(std::string *ret);
	bool getChar(char *ret);
	bool getUChar(unsigned char *ret);
	bool getVoidPtr(void **ret);
	bool getVarInt(int64_t* ret);

	unsigned int getSize(void);
	unsigned int getLeft(void);
	unsigned int getStreampos(void);
	void setStreampos(unsigned int spos);
	const char *getDataPtr(void);
	const char *getCurrDataPtr(void);
	bool incrementPtr(unsigned int amount);

private:

	const char* data;
	size_t streampos;
	size_t datalen;
	
	bool copy;
};


#endif //DATA_H_
