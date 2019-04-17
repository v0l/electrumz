#pragma once

#include <streambuf>
#include <istream>

struct membuf : std::streambuf {
	membuf() : std::streambuf() {

	}
	membuf(char const* base, size_t size) {
		char* p(const_cast<char*>(base));
		this->setg(p, p, p + size);
	}
	char* pbase() {
		return std::streambuf::pbase();
	}
	size_t psize() {
		return this->pptr() - this->pbase();
	}
	int write(char* data, size_t len) {
		return this->sputn(data, len);
	}
	int read(char* data, size_t len) {
		return this->sgetn(data, len);
	}
};