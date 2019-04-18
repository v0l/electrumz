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
		TXO(uint256 sHash, uint256 txHash, uint32_t tIndex, CAmount v)
			: scriptHash(sHash), txHash(txHash), n(tIndex), value(v) {
			blockHeight = 0;
		}

		TXO(uint256 sHash, uint256 txHash, uint32_t tIndex, CAmount v, unsigned int h)
			: scriptHash(sHash), txHash(txHash), n(tIndex), value(v), blockHeight(h) {
		}

		ADD_SERIALIZE_METHODS;

		template <typename Stream, typename Operation>
		inline void SerializationOp(Stream& s, Operation ser_action) {
			READWRITE(txHash);
			READWRITE(n);
			READWRITE(blockHeight);
			READWRITE(value);
			READWRITE(spendingTxi);
		}

		uint256 scriptHash;
		uint256 txHash;
		uint32_t n;
		uint64_t blockHeight;
		CAmount value;
		COutPoint spendingTxi;

		static const uint32_t ApproxSize = 147;
	};
}