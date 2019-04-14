// Copyright (c) 2014-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <electrumz/bitcoin/key_io.h>

#include <electrumz/bitcoin/base58.h>
#include <electrumz/bitcoin/bech32.h>
#include <electrumz/bitcoin/script.h>
#include <electrumz/bitcoin/util_strencodings.h>

#include <variant>

std::string EncodeDestination(const CTxDestination& dest)
{
	return std::visit([](auto&& arg) -> std::string {
		using T = std::decay_t<decltype(arg)>;
		if constexpr (std::is_same_v<T, CNoDestination>) {
			return {};
		}
		else if constexpr (std::is_same_v<T, CKeyID>) {
			std::vector<unsigned char> data = std::vector<unsigned char>(1, 0);
			data.insert(data.end(), arg.begin(), arg.end());
			return EncodeBase58Check(data);
		}
		else if constexpr (std::is_same_v<T, CScriptID>) {
			std::vector<unsigned char> data = std::vector<unsigned char>(1, 5);
			data.insert(data.end(), arg.begin(), arg.end());
			return EncodeBase58Check(data);
		}
		else if constexpr (std::is_same_v<T, WitnessV0KeyHash>) {
			std::vector<unsigned char> data = { 0 };
			data.reserve(33);
			ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, arg.begin(), arg.end());
			return bech32::Encode("bc", data);
		}
		else if constexpr (std::is_same_v<T, WitnessV0ScriptHash>) {
			std::vector<unsigned char> data = { 0 };
			data.reserve(53);
			ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, arg.begin(), arg.end());
			return bech32::Encode("bc", data);
		}
		else if constexpr (std::is_same_v<T, WitnessUnknown>) {
			if (arg.version < 1 || arg.version > 16 || arg.length < 2 || arg.length > 40) {
				return {};
			}
			std::vector<unsigned char> data = { (unsigned char)arg.version };
			data.reserve(1 + (arg.length * 8 + 4) / 5);
			ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, arg.program, arg.program + arg.length);
			return bech32::Encode("bc", data);
		}
		else {
			static_assert(always_false<T>::value, "non-exhaustive visitor!");
		}
	}, dest);
}