#include <electrumz/TXODB.h>

#include <spdlog/spdlog.h>
#include <electrumz/btc/def.h>
#include <electrumz/btc/block.h>
#include <electrumz/btc/hash.h>

#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <sstream>

using namespace electrumz::blockchain;
using namespace electrumz::bitcoin;

#define DEFAULT_MAPSIZE 1024 * 1024 * 512
#define MAX_BLOCK_SERIALIZED_SIZE 4000000

TXODB::TXODB(std::string path) {
	int err = 0;
	if (err = mdb_env_create(&this->env)) {
		spdlog::error("mdb create failed {}", mdb_strerror(err));
		return;
	}
	if (err = mdb_env_set_maxdbs(this->env, 2)) {
		spdlog::error("Failed to set max dbs: {}", mdb_strerror(err));
		return;
	}
	if (err = mdb_env_set_mapsize(this->env, DEFAULT_MAPSIZE)) {
		spdlog::error("Failed to set mapsize: {}", mdb_strerror(err));
		return;
	}
	if (err = mdb_env_open(this->env, path.c_str(), MDB_NOTLS | MDB_CREATE | MDB_WRITEMAP, NULL)) {
		spdlog::error("mdb open failed: {}", mdb_strerror(err));
		return;
	}
}

int TXODB::GetTXOs(MDB_txn* txn, MDB_dbi* dbi, sha256 scriptHash, txos &ntx) {
	int err = 0;
	if (dbi == nullptr) {
		err = mdb_dbi_open(txn, "txo", MDB_CREATE, dbi);
	}

	if (err != 0) {
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		return 0;
	}

	MDB_val key{
		BITCOIN_HASH_BYTES,
		scriptHash.Data()
	};

	MDB_val val;
	err = mdb_get(txn, *dbi, &key, &val);
	if (err == MDB_NOTFOUND) {
		return 2; //not found, (1 is ok)
	}
	else if (err == EINVAL) {
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		return 0;
	}
	else {
		ntx.ParseFromArray(val.mv_data, val.mv_size);
		return 1;
	}
}

int TXODB::WriteTXOs(MDB_txn *txn, MDB_dbi *dbi, sha256 scriptHash, txos &txns) {
	int err = 0;
	if (dbi == nullptr) {
		err = mdb_dbi_open(txn, "txo", MDB_CREATE, dbi);
	}

	MDB_val key{ 
		BITCOIN_HASH_BYTES,
		scriptHash.Data()
	};
	
	MDB_val val = {
		txns.ByteSize(),
		malloc(txns.ByteSize())
	};
	txns.SerializeToArray(val.mv_data, val.mv_size);

	err = mdb_put(txn, *dbi, &key, &val, MDB_NODUPDATA);
	if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL) {
		spdlog::error("WriteTXOs failed {}", mdb_strerror(err));
		return 0;
	}
	else {
		return 1;
	}
}


int TXODB::AddTXO(txo nTx, MDB_txn* txn, MDB_dbi *dbi) {
	int err = 0;
	if (dbi == nullptr) {
		if (!(err = mdb_dbi_open(txn, "txo", MDB_CREATE, dbi))) {
			spdlog::error("Error getting txos {}", mdb_strerror(err));
			return 0;
		}
	}
	
	MDB_val key {
		BITCOIN_HASH_BYTES,
		(void*)nTx.scripthash().c_str()
	};

	MDB_val val;
	err = mdb_get(txn, *dbi, &key, &val);
	if (err == MDB_NOTFOUND) {
		return 2; //not found, (1 is ok)
	}
	else if (err == EINVAL) {
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		return 0;
	}
	else {
		txos vtx;
		vtx.ParseFromArray(val.mv_data, val.mv_size);
		vtx.mutable_txns()->AddAllocated(&nTx);

		MDB_val val_write = {
			vtx.ByteSize(),
			malloc(vtx.ByteSize())
		};
		vtx.SerializeToArray(val_write.mv_data, val_write.mv_size);

		err = mdb_put(txn, *dbi, &key, &val_write, MDB_NODUPDATA);
		if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL) {
			free(val_write.mv_data);
			spdlog::error("AddTXOs write failed {}", mdb_strerror(err));
			return 0;
		}
		else {
			free(val_write.mv_data);
			return 1;
		}
		return err;
	}
	return 0;
}

void TXODB::PreLoadBlocks(std::string path) {
	spdlog::info("Preload starting.. this will take some time.");

	//dont automatically sync for pre-loading
	mdb_env_set_flags(this->env, MDB_NOSYNC, 1);

	auto block_buf = (unsigned char*)malloc(MAX_BLOCK_SERIALIZED_SIZE);
	Block blk;

	for (const auto & entry : std::filesystem::directory_iterator(path)) {
		auto fn = entry.path().filename().string();
		if (fn.find("blk") == 0) {

			//sync after we finished with the previous file
			mdb_env_sync(this->env, 0);

			spdlog::info("Loading block file {}", fn);

			std::ifstream bf(entry.path().string(), std::ios::binary);
			
			//this is not the actual hight, since blocks are not in order
			//but its approx for reporting purposes
			uint32_t height = 0; 
			
			char net_magic[4];
			uint32_t len;

			while (1) {
				len = 0;
				blk.SetNull();

				MDB_txn *txn;
				MDB_dbi dbi;
				mdb_txn_begin(this->env, nullptr, 0, &txn);
				mdb_dbi_open(txn, "txo", MDB_CREATE, &dbi);

				if (bf.eof()) {
					break;
				}
				try {
					bf >> net_magic;
					bf >> len;

					if (len > 0) {
						bf.read((char*)block_buf, len);
						if (bf.fail()) {
							spdlog::warn("Failed to read {} bytes got {} instead..", len, bf.gcount());
							continue;
						}

						sha256 blkHash = blk.GetHash();
						for (auto tx : blk.vtx) {
							sha256 txHash = tx.GetHash();
							int txop = 0;

							for (auto ntxo : tx.vout) {
								sha256 h = SHash(ntxo.scriptPubKey.begin(), ntxo.scriptPubKey.end());
								txo t;
								t.set_scripthash((char*)h.Data());
								t.set_txhash((char*)txHash.Data());
								t.set_blockhash((char*)blkHash.Data());
								t.set_txoindex(txop);
								t.set_value(ntxo.nValue);
								
								this->AddTXO(t, txn, &dbi);

								txop++;
							}
						}
					}
					else {
						spdlog::warn("Invalid len {}", len);
					}
				}
				catch (std::ios_base::failure ex) {
					spdlog::error("{}", ex.what());
				}
				mdb_txn_commit(txn);
			}

			mdb_env_sync(this->env, 0);
		}
	}
	spdlog::info("Preload finished!");
}