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
			TXO_RESIZED
		};

		class TXODB {
		public:
			TXODB(std::string);
			char* GetLMDBVersion() { return mdb_version(NULL, NULL, NULL); }
			void PreLoadBlocks(std::string);
			
			int GetTXOs(uint256, std::vector<TXO>&);
			int WriteTXOs(uint256, std::vector<TXO>&);
		private:
			MDB_env *env;
			MDB_dbi *txo_dbi;
			std::mutex txdbMutex;

			int InternalAddTXO(TXO&&);
			int StartTXOTxn(MDB_txn**, MDB_dbi&);
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