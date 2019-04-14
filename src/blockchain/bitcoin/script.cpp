// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2019 v0l
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <electrumz/bitcoin/script.h>
#include <electrumz/bitcoin/crypto_common.h>
#include <electrumz/bitcoin/util_strencodings.h>

bool CScript::IsPayToScriptHash() const
{
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 &&
            (*this)[0] == OP_HASH160 &&
            (*this)[1] == 0x14 &&
            (*this)[22] == OP_EQUAL);
}

bool CScript::IsPayToWitnessScriptHash() const
{
    // Extra-fast test for pay-to-witness-script-hash CScripts:
    return (this->size() == 34 &&
            (*this)[0] == OP_0 &&
            (*this)[1] == 0x20);
}

// A witness program is any valid CScript that consists of a 1-byte push opcode
// followed by a data push between 2 and 40 bytes.
bool CScript::IsWitnessProgram(int& version, std::vector<unsigned char>& program) const
{
    if (this->size() < 4 || this->size() > 42) {
        return false;
    }
    if ((*this)[0] != OP_0 && ((*this)[0] < OP_1 || (*this)[0] > OP_16)) {
        return false;
    }
    if ((size_t)((*this)[1] + 2) == this->size()) {
        version = DecodeOP_N((opcodetype)(*this)[0]);
        program = std::vector<unsigned char>(this->begin() + 2, this->end());
        return true;
    }
    return false;
}

bool CScript::IsPushOnly(const_iterator pc) const
{
    while (pc < end())
    {
        opcodetype opcode;
        if (!GetOp(pc, opcode))
            return false;
        // Note that IsPushOnly() *does* consider OP_RESERVED to be a
        // push-type opcode, however execution of OP_RESERVED fails, so
        // it's not relevant to P2SH/BIP62 as the scriptSig would fail prior to
        // the P2SH special validation code being executed.
        if (opcode > OP_16)
            return false;
    }
    return true;
}

bool CScript::IsPushOnly() const
{
    return this->IsPushOnly(begin());
}

std::string CScriptWitness::ToString() const
{
	std::string ret = "CScriptWitness(";
	for (unsigned int i = 0; i < stack.size(); i++) {
		if (i) {
			ret += ", ";
		}
		ret += HexStr(stack[i]);
	}
	return ret + ")";
}

bool GetScriptOp(CScriptBase::const_iterator& pc, CScriptBase::const_iterator end, opcodetype& opcodeRet, std::vector<unsigned char>* pvchRet)
{
    opcodeRet = OP_INVALIDOPCODE;
    if (pvchRet)
        pvchRet->clear();
    if (pc >= end)
        return false;

    // Read instruction
    if (end - pc < 1)
        return false;
    unsigned int opcode = *pc++;

    // Immediate operand
    if (opcode <= OP_PUSHDATA4)
    {
        unsigned int nSize = 0;
        if (opcode < OP_PUSHDATA1)
        {
            nSize = opcode;
        }
        else if (opcode == OP_PUSHDATA1)
        {
            if (end - pc < 1)
                return false;
            nSize = *pc++;
        }
        else if (opcode == OP_PUSHDATA2)
        {
            if (end - pc < 2)
                return false;
            nSize = ReadLE16(&pc[0]);
            pc += 2;
        }
        else if (opcode == OP_PUSHDATA4)
        {
            if (end - pc < 4)
                return false;
            nSize = ReadLE32(&pc[0]);
            pc += 4;
        }
        if (end - pc < 0 || (unsigned int)(end - pc) < nSize)
            return false;
        if (pvchRet)
            pvchRet->assign(pc, pc + nSize);
        pc += nSize;
    }

    opcodeRet = static_cast<opcodetype>(opcode);
    return true;
}

unsigned int static GetLen(unsigned char chHeader)
{
	if (chHeader == 2 || chHeader == 3)
		return COMPRESSED_PUBLIC_KEY_SIZE;
	if (chHeader == 4 || chHeader == 6 || chHeader == 7)
		return PUBLIC_KEY_SIZE;
	return 0;
}

bool static ValidSize(const std::vector<unsigned char> &vch) {
	return vch.size() > 0 && GetLen(vch[0]) == vch.size();
}

static bool MatchPayToPubkey(const CScript& script, valtype& pubkey)
{
	if (script.size() == PUBLIC_KEY_SIZE + 2 && script[0] == PUBLIC_KEY_SIZE && script.back() == OP_CHECKSIG) {
		pubkey = valtype(script.begin() + 1, script.begin() + PUBLIC_KEY_SIZE + 1);
		return ValidSize(pubkey);
	}
	if (script.size() == COMPRESSED_PUBLIC_KEY_SIZE + 2 && script[0] == COMPRESSED_PUBLIC_KEY_SIZE && script.back() == OP_CHECKSIG) {
		pubkey = valtype(script.begin() + 1, script.begin() + COMPRESSED_PUBLIC_KEY_SIZE + 1);
		return ValidSize(pubkey);
	}
	return false;
}

static bool MatchPayToPubkeyHash(const CScript& script, valtype& pubkeyhash)
{
	if (script.size() == 25 && script[0] == OP_DUP && script[1] == OP_HASH160 && script[2] == 20 && script[23] == OP_EQUALVERIFY && script[24] == OP_CHECKSIG) {
		pubkeyhash = valtype(script.begin() + 3, script.begin() + 23);
		return true;
	}
	return false;
}

//Test for "small positive integer" script opcodes - OP_1 through OP_16. * /
static constexpr bool IsSmallInteger(opcodetype opcode)
{
	return opcode >= 0x51 && opcode <= 0x60;
}

static bool MatchMultisig(const CScript& script, unsigned int& required, std::vector<valtype>& pubkeys)
{
	opcodetype opcode;
	valtype data;
	CScript::const_iterator it = script.begin();
	if (script.size() < 1 || script.back() != OP_CHECKMULTISIG) return false;

	if (!script.GetOp(it, opcode, data) || !IsSmallInteger(opcode)) return false;
	required = CScript::DecodeOP_N(opcode);
	while (script.GetOp(it, opcode, data) && ValidSize(data)) {
		pubkeys.emplace_back(std::move(data));
	}
	if (!IsSmallInteger(opcode)) return false;
	unsigned int keys = CScript::DecodeOP_N(opcode);
	if (pubkeys.size() != keys || keys < required) return false;
	return (it + 1 == script.end());
}

txnouttype Solver(const CScript& scriptPubKey, std::vector<std::vector<unsigned char>>& vSolutionsRet)
{
	vSolutionsRet.clear();

	// Shortcut for pay-to-script-hash, which are more constrained than the other types:
	// it is always OP_HASH160 20 [20 byte hash] OP_EQUAL
	if (scriptPubKey.IsPayToScriptHash())
	{
		std::vector<unsigned char> hashBytes(scriptPubKey.begin() + 2, scriptPubKey.begin() + 22);
		vSolutionsRet.push_back(hashBytes);
		return TX_SCRIPTHASH;
	}

	int witnessversion;
	std::vector<unsigned char> witnessprogram;
	if (scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram)) {
		if (witnessversion == 0 && witnessprogram.size() == WITNESS_V0_KEYHASH_SIZE) {
			vSolutionsRet.push_back(witnessprogram);
			return TX_WITNESS_V0_KEYHASH;
		}
		if (witnessversion == 0 && witnessprogram.size() == WITNESS_V0_SCRIPTHASH_SIZE) {
			vSolutionsRet.push_back(witnessprogram);
			return TX_WITNESS_V0_SCRIPTHASH;
		}
		if (witnessversion != 0) {
			vSolutionsRet.push_back(std::vector<unsigned char>{(unsigned char)witnessversion});
			vSolutionsRet.push_back(std::move(witnessprogram));
			return TX_WITNESS_UNKNOWN;
		}
		return TX_NONSTANDARD;
	}

	// Provably prunable, data-carrying output
	//
	// So long as script passes the IsUnspendable() test and all but the first
	// byte passes the IsPushOnly() test we don't care what exactly is in the
	// script.
	if (scriptPubKey.size() >= 1 && scriptPubKey[0] == OP_RETURN && scriptPubKey.IsPushOnly(scriptPubKey.begin() + 1)) {
		return TX_NULL_DATA;
	}

	std::vector<unsigned char> data;
	if (MatchPayToPubkey(scriptPubKey, data)) {
		vSolutionsRet.push_back(std::move(data));
		return TX_PUBKEY;
	}

	if (MatchPayToPubkeyHash(scriptPubKey, data)) {
		vSolutionsRet.push_back(std::move(data));
		return TX_PUBKEYHASH;
	}

	unsigned int required;
	std::vector<std::vector<unsigned char>> keys;
	if (MatchMultisig(scriptPubKey, required, keys)) {
		vSolutionsRet.push_back({ static_cast<unsigned char>(required) }); // safe as required is in range 1..16
		vSolutionsRet.insert(vSolutionsRet.end(), keys.begin(), keys.end());
		vSolutionsRet.push_back({ static_cast<unsigned char>(keys.size()) }); // safe as size is in range 1..16
		return TX_MULTISIG;
	}

	vSolutionsRet.clear();
	return TX_NONSTANDARD;
}

bool ExtractDestination(const CScript& scriptPubKey, CTxDestination& addressRet)
{
	std::vector<valtype> vSolutions;
	txnouttype whichType = Solver(scriptPubKey, vSolutions);

	if (whichType == TX_PUBKEY) {
		auto vch = vSolutions[0];
		if (vch.size() <= 0) {
			return false;
		}

		addressRet = CKeyID(Hash160(vch));
		return true;
	}
	else if (whichType == TX_PUBKEYHASH)
	{
		addressRet = CKeyID(uint160(vSolutions[0]));
		return true;
	}
	else if (whichType == TX_SCRIPTHASH)
	{
		addressRet = CScriptID(uint160(vSolutions[0]));
		return true;
	}
	else if (whichType == TX_WITNESS_V0_KEYHASH) {
		WitnessV0KeyHash hash;
		std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
		addressRet = hash;
		return true;
	}
	else if (whichType == TX_WITNESS_V0_SCRIPTHASH) {
		WitnessV0ScriptHash hash;
		std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
		addressRet = hash;
		return true;
	}
	else if (whichType == TX_WITNESS_UNKNOWN) {
		WitnessUnknown unk;
		unk.version = vSolutions[0][0];
		std::copy(vSolutions[1].begin(), vSolutions[1].end(), unk.program);
		unk.length = vSolutions[1].size();
		addressRet = unk;
		return true;
	}
	// Multisig txns have more than one address...
	return false;
}

bool ExtractDestinations(const CScript& scriptPubKey, txnouttype& typeRet, std::vector<CTxDestination>& addressRet, int& nRequiredRet)
{
	addressRet.clear();
	std::vector<valtype> vSolutions;
	typeRet = Solver(scriptPubKey, vSolutions);
	if (typeRet == TX_NONSTANDARD) {
		return false;
	}
	else if (typeRet == TX_NULL_DATA) {
		// This is data, not addresses
		return false;
	}

	if (typeRet == TX_MULTISIG)
	{
		nRequiredRet = vSolutions.front()[0];
		for (unsigned int i = 1; i < vSolutions.size() - 1; i++)
		{
			auto vch = vSolutions[i];
			if (vch.size() <= 0) {
				return false;
			}

			CTxDestination address = CKeyID(Hash160(vch));
			addressRet.push_back(address);
		}

		if (addressRet.empty())
			return false;
	}
	else
	{
		nRequiredRet = 1;
		CTxDestination address;
		if (!ExtractDestination(scriptPubKey, address))
			return false;
		addressRet.push_back(address);
	}

	return true;
}