#pragma once
#include <string>

class Buffer :public std::string
{
public:
	Buffer() :std::string() {}
	Buffer(size_t size) :std::string() { resize(size); }
	Buffer(const std::string& str) :std::string(str) {}
	Buffer(const char* str) :std::string(str) {}
	Buffer(const char* str, size_t length)
		:std::string() {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	Buffer(const char* begin, const char* end) :std::string() {
		long int len = end - begin;
		if (len > 0) {
			resize(len);
			memcpy((char*)c_str(), begin, len);
		}
	}
	operator void* () { return (char*)c_str(); }
	Buffer& operator=(const Buffer& other) {
		if (this != &other) {
			*(std::string*)this = other;
		}
		return *this;
	}
	operator char* () { return (char*)c_str(); }
	operator unsigned char* () { return (unsigned char*)c_str(); }
	operator char* () const { return (char*)c_str(); }
	operator const char* () const { return c_str(); }
	operator const void* () const { return c_str(); }
	void Update(const char* str, size_t length) {
		resize(length);
		memcpy((char*)c_str(), str, length);
	}
	void Zero() {
		memset((char*)c_str(), 0, size());
	}
};