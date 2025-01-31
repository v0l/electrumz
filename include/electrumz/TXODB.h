#pragma once

#include <electrumz/bitcoin/uint256.h>
#include <electrumz/bitcoin/block.h>
#include <electrumz/TXO.h>

#include <vector>
#include <mutex>
#include <string>
#include <lmdb.h>

#define DBI_TXO "txo"
#define DBI_ADDR "addr"
#define DBI_BLK "blk"

namespace electrumz {
	namespace blockchain {
		enum TXODB_ERR {
			TXO_ERR,
			TXO_OK,
			TXO_NOTFOUND,
			TXO_RESIZED,
			TXO_MAP_FULL
		};

		class TXODB {
		public:
			TXODB(std::string);
			int Open();
			char* GetLMDBVersion() { return mdb_version(NULL, NULL, NULL); }
			void PreLoadBlocks(std::string);
			
			int GetTXOs(uint256, std::vector<TXO>&);
			int GetTXOStats(MDB_stat* stats, const char* dbn);
		private:

			std::string dbPath;
			MDB_env *env;
			std::mutex resize_lock;

			/**
			 * Appends a new UTXO to the database.
			*/
			int InternalAddTXO(TXO&, MDB_txn*, MDB_dbi&, MDB_dbi&);
			
			/**
			 * Marks an output as spent by txin.prevout
			*/
			int InternalSpendTXO(COutPoint&, COutPoint&);
			int StartTXOTxn(MDB_txn**, const char*, MDB_dbi&);
			int IncreaseMapSize();
			int PushBlockTip(MDB_txn*, const CBlockHeader&);
		};

		template<typename Stream> inline void Serialize(Stream &s, MDB_val obj)
		{
			s.write((char*)&obj.mv_data, obj.mv_size);
		}

		template<typename Stream> inline void Unserialize(Stream& s, MDB_val& a) {
			s.read((char*)a.mv_data, a.mv_size);
		}
	}
}