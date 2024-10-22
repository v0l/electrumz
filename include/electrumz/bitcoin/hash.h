// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 v0l
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_H
#define BITCOIN_HASH_H

#include <electrumz/bitcoin/version.h>
#include <electrumz/bitcoin/uint256.h>
#include <electrumz/bitcoin/crypto_common.h>
#include <electrumz/bitcoin/prevector.h>

#include <mbedtls/sha256.h>
#include <mbedtls/ripemd160.h>

static const size_t SHA256_OUTPUT_SIZE = 32;

/** A hasher class for 256-bit hash (single SHA-256). */
class CSHash256 {
private:
	mbedtls_sha256_context sha;
public:
	static const size_t OUTPUT_SIZE = 32;

	CSHash256() {
		mbedtls_sha256_init(&this->sha);
		mbedtls_sha256_starts_ret(&this->sha, 0);
	}

	~CSHash256() {
		mbedtls_sha256_free(&this->sha);
	}

	void Finalize(unsigned char hash[OUTPUT_SIZE]) {
		mbedtls_sha256_finish_ret(&this->sha, hash);
	}

	CSHash256& Write(const unsigned char *data, size_t len) {
		mbedtls_sha256_update_ret(&this->sha, data, len);
		return *this;
	}

	CSHash256& Reset() {
		mbedtls_sha256_free(&this->sha);
		return *this;
	}
};

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256 {
private:
	mbedtls_sha256_context sha;
public:
	static const size_t OUTPUT_SIZE = 32;

	CHash256() {
		mbedtls_sha256_init(&this->sha);
		mbedtls_sha256_starts_ret(&this->sha, 0);
	}

	~CHash256() {
		mbedtls_sha256_free(&this->sha);
	}

	void Finalize(unsigned char hash[OUTPUT_SIZE]) {
		unsigned char buf[OUTPUT_SIZE];
		mbedtls_sha256_finish_ret(&this->sha, buf);
		
		//so second round
		mbedtls_sha256_free(&this->sha);
		mbedtls_sha256_starts_ret(&this->sha, 0);
		mbedtls_sha256_update_ret(&this->sha, buf, OUTPUT_SIZE);
		mbedtls_sha256_finish_ret(&this->sha, hash);
	}

	CHash256& Write(const unsigned char *data, size_t len) {
		mbedtls_sha256_update_ret(&this->sha, data, len);
		return *this;
	}

	CHash256& Reset() {
		mbedtls_sha256_free(&this->sha);
		return *this;
	}
};

/** A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160). */
class CHash160 {
private:
	mbedtls_sha256_context sha_ctx;
	mbedtls_ripemd160_context ctx;
public:
	static const size_t OUTPUT_SIZE = 20;

	CHash160() {
		mbedtls_sha256_init(&this->sha_ctx);
		mbedtls_sha256_starts_ret(&this->sha_ctx, 0);

		mbedtls_ripemd160_init(&this->ctx);
		mbedtls_ripemd160_starts_ret(&this->ctx);
	}

	~CHash160() {
		mbedtls_sha256_free(&this->sha_ctx);
		mbedtls_ripemd160_free(&this->ctx);
	}

	void Finalize(unsigned char hash[OUTPUT_SIZE]) {
		unsigned char buf[CHash256::OUTPUT_SIZE];

		mbedtls_sha256_finish_ret(&this->sha_ctx, buf);
		mbedtls_ripemd160_update(&this->ctx, buf, CHash256::OUTPUT_SIZE);
		mbedtls_ripemd160_finish_ret(&this->ctx, hash);
	}

	CHash160& Write(const unsigned char *data, size_t len) {
		mbedtls_sha256_update_ret(&this->sha_ctx, data, len);
		return *this;
	}

	CHash160& Reset() {
		mbedtls_sha256_free(&this->sha_ctx);
		mbedtls_ripemd160_free(&this->ctx);
		return *this;
	}
};

/** Compute the 256-bit hash of an object. */
template<typename T1>
inline uint256 SHash(const T1 pbegin, const T1 pend)
{
	static const unsigned char pblank[1] = {};
	uint256 result;
	CSHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
		.Finalize((unsigned char*)&result);
	return result;
}

/** Compute the 256-bit hash of an object. */
template<typename T1>
inline uint256 Hash(const T1 pbegin, const T1 pend)
{
	static const unsigned char pblank[1] = {};
	uint256 result;
	CHash256().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
		.Finalize((unsigned char*)&result);
	return result;
}

/** Compute the 256-bit hash of the concatenation of two objects. */
template<typename T1, typename T2>
inline uint256 Hash(const T1 p1begin, const T1 p1end,
	const T2 p2begin, const T2 p2end) {
	static const unsigned char pblank[1] = {};
	uint256 result;
	CHash256().Write(p1begin == p1end ? pblank : (const unsigned char*)&p1begin[0], (p1end - p1begin) * sizeof(p1begin[0]))
		.Write(p2begin == p2end ? pblank : (const unsigned char*)&p2begin[0], (p2end - p2begin) * sizeof(p2begin[0]))
		.Finalize((unsigned char*)&result);
	return result;
}

/** Compute the 160-bit hash an object. */
template<typename T1>
inline uint160 Hash160(const T1 pbegin, const T1 pend)
{
	static unsigned char pblank[1] = {};
	uint160 result;
	CHash160().Write(pbegin == pend ? pblank : (const unsigned char*)&pbegin[0], (pend - pbegin) * sizeof(pbegin[0]))
		.Finalize((unsigned char*)&result);
	return result;
}

/** Compute the 160-bit hash of a vector. */
inline uint160 Hash160(const std::vector<unsigned char>& vch)
{
	return Hash160(vch.begin(), vch.end());
}

/** Compute the 160-bit hash of a vector. */
template<unsigned int N>
inline uint160 Hash160(const prevector<N, unsigned char>& vch)
{
	return Hash160(vch.begin(), vch.end());
}

/** A writer stream (for serialization) that computes a 256-bit hash. */
class CHashWriter
{
private:
	CHash256 ctx;

	const int nType;
	const int nVersion;
public:

	CHashWriter(int nTypeIn, int nVersionIn) : nType(nTypeIn), nVersion(nVersionIn) {}

	int GetType() const { return nType; }
	int GetVersion() const { return nVersion; }

	void write(const char *pch, size_t size) {
		ctx.Write((const unsigned char*)pch, size);
	}

	// invalidates the object
	uint256 GetHash() {
		uint256 result;
		ctx.Finalize((unsigned char*)&result);
		return result;
	}

	/**
	 * Returns the first 64 bits from the resulting hash.
	 */
	inline uint64_t GetCheapHash() {
		unsigned char result[CHash256::OUTPUT_SIZE];
		ctx.Finalize(result);
		return ReadLE64(result);
	}

	template<typename T>
	CHashWriter& operator<<(const T& obj) {
		// Serialize to this stream
		::Serialize(*this, obj);
		return (*this);
	}
};

/** Compute the 256-bit hash of an object's serialization. */
template<typename T>
uint256 SerializeHash(const T& obj, int nType = SER_GETHASH, int nVersion = PROTOCOL_VERSION)
{
	CHashWriter ss(nType, nVersion);
	ss << obj;
	return ss.GetHash();
}

#endif BITCOIN_HASH_H