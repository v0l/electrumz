#pragma once

#include <electrumz/bitcoin/serialize.h>
#include <electrumz/bitcoin/uint256.h>
#include <electrumz/bitcoin/amount.h>

#include <mutex>
#include <string>
#include <lmdb.h>

namespace electrumz {
	namespace blockchain {
		class TXO {
		public:
			TXO() {

			}

			//scriptHash, txHash, txIndex, value
			TXO(uint256 sHash, uint256 txHash, unsigned int tIndex, CAmount v)
				: scriptHash(sHash), txHash(txHash), txoIndex(tIndex), value(v) {
				blockHeight = 0;
			}

			TXO(uint256 sHash, uint256 txHash, unsigned int tIndex, CAmount v, unsigned int h)
				: scriptHash(sHash), txHash(txHash), txoIndex(tIndex), value(v), blockHeight(h) {
			}

			ADD_SERIALIZE_METHODS;

			template <typename Stream, typename Operation>
			inline void SerializationOp(Stream& s, Operation ser_action) {
				READWRITE(txHash);
				READWRITE(txoIndex);
				READWRITE(blockHeight);
				READWRITE(value);
				READWRITE(spendingTxi);
			}

			uint256 scriptHash;
			uint256 txHash;
			unsigned short txoIndex;
			unsigned int blockHeight;
			CAmount value;
			std::pair<uint256, unsigned short> spendingTxi;
		};

		class TXODB {
		public:
			TXODB(std::string);
			char* GetLMDBVersion() { return mdb_version(NULL, NULL, NULL); }
			void PreLoadBlocks(std::string);
			int GetTx(MDB_txn**);
			int AddTXO(TXO);
			int GetTXOs(MDB_txn*, uint256, std::vector<TXO>&);
			int WriteTXOs(MDB_txn*, uint256, std::vector<TXO>);
		private:
			MDB_env *env;
			MDB_dbi *txo_dbi;
			std::mutex txdbMutex;
		};
	}
}