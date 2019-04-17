#pragma once

#define BITCOIN_HASH_BYTES 32

#define ADD_SERIALABLE_FUNC_DEF			\
	int Read(unsigned char*, size_t);	\
	int Write(unsigned char*, size_t);	\
	void SetNull();						\

namespace electrumz {
	class Serializable {
	public:
		virtual int Read(unsigned char*, size_t) = 0;
		virtual int Write(unsigned char*, size_t) = 0;
		virtual void SetNull() = 0;
	};

	class sha256 : Serializable {
	private:
		uint8_t data[BITCOIN_HASH_BYTES];
	public:
		sha256() { this->SetNull(); }
		sha256(unsigned char*);

		ADD_SERIALABLE_FUNC_DEF;
		unsigned char* Data() { return (unsigned char*)&this->data; }
	};
}