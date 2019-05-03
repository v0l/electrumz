#pragma once

#include <electrumz/bitcoin/serialize.h>
#include <electrumz/bitcoin/amount.h>
#include <electrumz/bitcoin/uint256.h>
#include <electrumz/bitcoin/transaction.h>

namespace electrumz {
	class TXO {
	public:
		TXO() {

		}

		//scriptHash, txHash, txIndex, value
		TXO(uint256 sHash, uint256 txHash, uint32_t tIndex, CAmount v, uint64_t blockHeight)
			: scriptHash(sHash), txHash(txHash), n(tIndex), value(v), blockHeight(blockHeight) {
		}

		ADD_SERIALIZE_METHODS;

		template <typename Stream, typename Operation>
		inline void SerializationOp(Stream& s, Operation ser_action) {
			READWRITE(n);
			READWRITE(value);
			READWRITE(blockHeight);
			READWRITE(spend);
		}
		
		//in memory only
		uint256 txHash;
		uint256 scriptHash;

		uint32_t n;
		CAmount value;
		uint64_t blockHeight;
		COutPoint spend;

		static const uint32_t ApproxSize = sizeof(n) + sizeof(value) + sizeof(blockHeight) + sizeof(spend);
	};
}