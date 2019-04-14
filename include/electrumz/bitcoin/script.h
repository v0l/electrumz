// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 v0l
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SCRIPT_H
#define BITCOIN_SCRIPT_SCRIPT_H


#include <variant>
#include <electrumz/bitcoin/prevector.h>
#include <electrumz/bitcoin/serialize.h>
#include <electrumz/bitcoin/hash.h>

// Maximum script length in bytes
static const int MAX_SCRIPT_SIZE = 10000;
static constexpr size_t WITNESS_V0_SCRIPTHASH_SIZE = 32;
static constexpr size_t WITNESS_V0_KEYHASH_SIZE = 20;
static constexpr unsigned int PUBLIC_KEY_SIZE = 65;
static constexpr unsigned int COMPRESSED_PUBLIC_KEY_SIZE = 33;
static constexpr unsigned int SIGNATURE_SIZE = 72;
static constexpr unsigned int COMPACT_SIGNATURE_SIZE = 65;

enum txnouttype
{
	TX_NONSTANDARD,
	// 'standard' transaction types:
	TX_PUBKEY,
	TX_PUBKEYHASH,
	TX_SCRIPTHASH,
	TX_MULTISIG,
	TX_NULL_DATA, //!< unspendable OP_RETURN script that carries data
	TX_WITNESS_V0_SCRIPTHASH,
	TX_WITNESS_V0_KEYHASH,
	TX_WITNESS_UNKNOWN, //!< Only for Witness versions not already defined above
};

/** Script opcodes */
enum opcodetype
{
	// push value
	OP_0 = 0x00,
	OP_FALSE = OP_0,
	OP_PUSHDATA1 = 0x4c,
	OP_PUSHDATA2 = 0x4d,
	OP_PUSHDATA4 = 0x4e,
	OP_1NEGATE = 0x4f,
	OP_RESERVED = 0x50,
	OP_1 = 0x51,
	OP_TRUE = OP_1,
	OP_2 = 0x52,
	OP_3 = 0x53,
	OP_4 = 0x54,
	OP_5 = 0x55,
	OP_6 = 0x56,
	OP_7 = 0x57,
	OP_8 = 0x58,
	OP_9 = 0x59,
	OP_10 = 0x5a,
	OP_11 = 0x5b,
	OP_12 = 0x5c,
	OP_13 = 0x5d,
	OP_14 = 0x5e,
	OP_15 = 0x5f,
	OP_16 = 0x60,

	// control
	OP_NOP = 0x61,
	OP_VER = 0x62,
	OP_IF = 0x63,
	OP_NOTIF = 0x64,
	OP_VERIF = 0x65,
	OP_VERNOTIF = 0x66,
	OP_ELSE = 0x67,
	OP_ENDIF = 0x68,
	OP_VERIFY = 0x69,
	OP_RETURN = 0x6a,

	// stack ops
	OP_TOALTSTACK = 0x6b,
	OP_FROMALTSTACK = 0x6c,
	OP_2DROP = 0x6d,
	OP_2DUP = 0x6e,
	OP_3DUP = 0x6f,
	OP_2OVER = 0x70,
	OP_2ROT = 0x71,
	OP_2SWAP = 0x72,
	OP_IFDUP = 0x73,
	OP_DEPTH = 0x74,
	OP_DROP = 0x75,
	OP_DUP = 0x76,
	OP_NIP = 0x77,
	OP_OVER = 0x78,
	OP_PICK = 0x79,
	OP_ROLL = 0x7a,
	OP_ROT = 0x7b,
	OP_SWAP = 0x7c,
	OP_TUCK = 0x7d,

	// splice ops
	OP_CAT = 0x7e,
	OP_SUBSTR = 0x7f,
	OP_LEFT = 0x80,
	OP_RIGHT = 0x81,
	OP_SIZE = 0x82,

	// bit logic
	OP_INVERT = 0x83,
	OP_AND = 0x84,
	OP_OR = 0x85,
	OP_XOR = 0x86,
	OP_EQUAL = 0x87,
	OP_EQUALVERIFY = 0x88,
	OP_RESERVED1 = 0x89,
	OP_RESERVED2 = 0x8a,

	// numeric
	OP_1ADD = 0x8b,
	OP_1SUB = 0x8c,
	OP_2MUL = 0x8d,
	OP_2DIV = 0x8e,
	OP_NEGATE = 0x8f,
	OP_ABS = 0x90,
	OP_NOT = 0x91,
	OP_0NOTEQUAL = 0x92,

	OP_ADD = 0x93,
	OP_SUB = 0x94,
	OP_MUL = 0x95,
	OP_DIV = 0x96,
	OP_MOD = 0x97,
	OP_LSHIFT = 0x98,
	OP_RSHIFT = 0x99,

	OP_BOOLAND = 0x9a,
	OP_BOOLOR = 0x9b,
	OP_NUMEQUAL = 0x9c,
	OP_NUMEQUALVERIFY = 0x9d,
	OP_NUMNOTEQUAL = 0x9e,
	OP_LESSTHAN = 0x9f,
	OP_GREATERTHAN = 0xa0,
	OP_LESSTHANOREQUAL = 0xa1,
	OP_GREATERTHANOREQUAL = 0xa2,
	OP_MIN = 0xa3,
	OP_MAX = 0xa4,

	OP_WITHIN = 0xa5,

	// crypto
	OP_RIPEMD160 = 0xa6,
	OP_SHA1 = 0xa7,
	OP_SHA256 = 0xa8,
	OP_HASH160 = 0xa9,
	OP_HASH256 = 0xaa,
	OP_CODESEPARATOR = 0xab,
	OP_CHECKSIG = 0xac,
	OP_CHECKSIGVERIFY = 0xad,
	OP_CHECKMULTISIG = 0xae,
	OP_CHECKMULTISIGVERIFY = 0xaf,

	// expansion
	OP_NOP1 = 0xb0,
	OP_CHECKLOCKTIMEVERIFY = 0xb1,
	OP_NOP2 = OP_CHECKLOCKTIMEVERIFY,
	OP_CHECKSEQUENCEVERIFY = 0xb2,
	OP_NOP3 = OP_CHECKSEQUENCEVERIFY,
	OP_NOP4 = 0xb3,
	OP_NOP5 = 0xb4,
	OP_NOP6 = 0xb5,
	OP_NOP7 = 0xb6,
	OP_NOP8 = 0xb7,
	OP_NOP9 = 0xb8,
	OP_NOP10 = 0xb9,

	OP_INVALIDOPCODE = 0xff,
};

class CScript;

class CKeyID : public uint160
{
public:
	CKeyID() : uint160() {}
	explicit CKeyID(const uint160& in) : uint160(in) {}
};

/** A reference to a CScript: the Hash160 of its serialization (see script.h) */
class CScriptID : public uint160
{
public:
	CScriptID() : uint160() {}
	explicit CScriptID(const CScript& in);
	CScriptID(const uint160& in) : uint160(in) {}
};

class CNoDestination {
public:
	friend bool operator==(const CNoDestination &a, const CNoDestination &b) { return true; }
	friend bool operator<(const CNoDestination &a, const CNoDestination &b) { return true; }
};

struct WitnessV0ScriptHash : public uint256
{
	WitnessV0ScriptHash() : uint256() {}
	explicit WitnessV0ScriptHash(const uint256& hash) : uint256(hash) {}
	explicit WitnessV0ScriptHash(const CScript& script);
	using uint256::uint256;
};

struct WitnessV0KeyHash : public uint160
{
	WitnessV0KeyHash() : uint160() {}
	explicit WitnessV0KeyHash(const uint160& hash) : uint160(hash) {}
	using uint160::uint160;
};

//! CTxDestination subtype to encode any future Witness version
struct WitnessUnknown
{
	unsigned int version;
	unsigned int length;
	unsigned char program[40];

	friend bool operator==(const WitnessUnknown& w1, const WitnessUnknown& w2) {
		if (w1.version != w2.version) return false;
		if (w1.length != w2.length) return false;
		return std::equal(w1.program, w1.program + w1.length, w2.program);
	}

	friend bool operator<(const WitnessUnknown& w1, const WitnessUnknown& w2) {
		if (w1.version < w2.version) return true;
		if (w1.version > w2.version) return false;
		if (w1.length < w2.length) return true;
		if (w1.length > w2.length) return false;
		return std::lexicographical_compare(w1.program, w1.program + w1.length, w2.program, w2.program + w2.length);
	}
};

typedef prevector<28, unsigned char> CScriptBase;
typedef std::vector<unsigned char> valtype;
typedef std::variant<CNoDestination, CKeyID, CScriptID, WitnessV0ScriptHash, WitnessV0KeyHash, WitnessUnknown> CTxDestination;

bool GetScriptOp(CScriptBase::const_iterator& pc, CScriptBase::const_iterator end, opcodetype& opcodeRet, std::vector<unsigned char>* pvchRet);

class CScript : public CScriptBase
{
public:
	CScript() { }
	CScript(const_iterator pbegin, const_iterator pend) : CScriptBase(pbegin, pend) { }
	CScript(std::vector<unsigned char>::const_iterator pbegin, std::vector<unsigned char>::const_iterator pend) : CScriptBase(pbegin, pend) { }
	CScript(const unsigned char* pbegin, const unsigned char* pend) : CScriptBase(pbegin, pend) { }

	ADD_SERIALIZE_METHODS;

	template <typename Stream, typename Operation>
	inline void SerializationOp(Stream& s, Operation ser_action) {
		READWRITEAS(CScriptBase, *this);
	}

	bool GetOp(const_iterator& pc, opcodetype& opcodeRet, std::vector<unsigned char>& vchRet) const
	{
		return GetScriptOp(pc, end(), opcodeRet, &vchRet);
	}

	bool GetOp(const_iterator& pc, opcodetype& opcodeRet) const
	{
		return GetScriptOp(pc, end(), opcodeRet, nullptr);
	}

	/** Encode/decode small integers: */
	static int DecodeOP_N(opcodetype opcode)
	{
		if (opcode == OP_0)
			return 0;
		assert(opcode >= OP_1 && opcode <= OP_16);
		return (int)opcode - (int)(OP_1 - 1);
	}
	static opcodetype EncodeOP_N(int n)
	{
		assert(n >= 0 && n <= 16);
		if (n == 0)
			return OP_0;
		return (opcodetype)(OP_1 + n - 1);
	}

	bool IsPayToScriptHash() const;
	bool IsPayToWitnessScriptHash() const;
	bool IsWitnessProgram(int& version, std::vector<unsigned char>& program) const;

	/** Called by IsStandardTx and P2SH/BIP62 VerifyScript (which makes it consensus-critical). */
	bool IsPushOnly(const_iterator pc) const;
	bool IsPushOnly() const;

	/** Check if the script contains valid OP_CODES */
	bool HasValidOps() const;

	/**
	 * Returns whether the script is guaranteed to fail at execution,
	 * regardless of the initial stack. This allows outputs to be pruned
	 * instantly when entering the UTXO set.
	 */
	bool IsUnspendable() const
	{
		return (size() > 0 && *begin() == OP_RETURN) || (size() > MAX_SCRIPT_SIZE);
	}

	void clear()
	{
		// The default prevector::clear() does not release memory
		CScriptBase::clear();
		shrink_to_fit();
	}
};

struct CScriptWitness
{
	// Note that this encodes the data elements being pushed, rather than
	// encoding them as a CScript that pushes them.
	std::vector<std::vector<unsigned char> > stack;

	// Some compilers complain without a default constructor
	CScriptWitness() { }

	bool IsNull() const { return stack.empty(); }

	void SetNull() { stack.clear(); stack.shrink_to_fit(); }

	std::string ToString() const;
};

unsigned int static GetLen(unsigned char chHeader);
bool static ValidSize(const std::vector<unsigned char> &vch);
static bool MatchPayToPubkey(const CScript& script, valtype& pubkey);
static bool MatchPayToPubkeyHash(const CScript& script, valtype& pubkeyhash);
static constexpr bool IsSmallInteger(opcodetype opcode);
static bool MatchMultisig(const CScript& script, unsigned int& required, std::vector<valtype>& pubkeys);
txnouttype Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet);
bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet);
bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet);

#endif BITCOIN_SCRIPT_SCRIPT_H