#pragma once

#include <electrumz/bitcoin/uint256.h>
#include <electrumz/TXO.h>

#include <vector>
#include <mutex>
#include <string>
#include <lmdb.h>

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
		private:
			static constexpr char* TXO_DBI = "txo";

			std::string dbPath;
			MDB_env *env;

			/**
			 * Appends a TXO to a scriptHash.
			*/
			int InternalAddTXO(TXO&, MDB_txn*, MDB_dbi&, MDB_dbi&);
			
			/**
			 * Marks an output as spent by txin.prevout
			*/
			int InternalSpendTXO(COutPoint&, COutPoint&);
			int StartTXOTxn(MDB_txn**, const char*, MDB_dbi&);
			int IncreaseMapSize();
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