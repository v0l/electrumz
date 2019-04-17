#pragma once

#include <electrumz/btc/def.h>
#include <txo.pb.h>

#include <mutex>
#include <string>
#include <lmdb.h>

namespace electrumz {
	namespace blockchain {
		class TXODB {
		public:
			TXODB(std::string);
			char* GetLMDBVersion() { return mdb_version(NULL, NULL, NULL); }
			void PreLoadBlocks(std::string);
			int AddTXO(txo, MDB_txn*, MDB_dbi*);
			int GetTXOs(MDB_txn*, MDB_dbi*, sha256, txos&);
			int WriteTXOs(MDB_txn*, MDB_dbi*, sha256, txos&);
		private:
			MDB_env *env;
			MDB_dbi *txo_dbi;
			std::mutex txdbMutex;
		};
	}
}