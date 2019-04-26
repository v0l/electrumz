#include <electrumz/TXODB.h>

#include <spdlog/spdlog.h>
#include <electrumz/bitcoin/block.h>
#include <electrumz/bitcoin/hash.h>
#include <electrumz/bitcoin/streams.h>

#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <sstream>
#include <queue>
#include <atomic>
#include <functional>

using namespace electrumz;
using namespace electrumz::blockchain;

constexpr auto MAPSIZE_BASE_UNIT = 1024 * 1024 * 1024;
#define PRELOAD_BUFFER_SIZE 0x8000000 //load full blockfiles in RAM, this is called "MAX_BLOCKFILE_SIZE" in Bitcoin Core

TXODB::TXODB(std::string path) {
	this->dbPath = path;

	std::filesystem::create_directories(this->dbPath);
}

int TXODB::Open() {
	int err = 0;
	if (err = mdb_env_create(&this->env)) {
		spdlog::error("mdb create failed {}", mdb_strerror(err));
		return err;
	}
	if (err = mdb_env_set_maxdbs(this->env, 2)) {
		spdlog::error("Failed to set max dbs: {}", mdb_strerror(err));
		return err;
	}
	if (err = mdb_env_open(this->env, this->dbPath.c_str(), MDB_NOTLS | MDB_CREATE | MDB_WRITEMAP, NULL)) {
		spdlog::error("mdb open failed: {}", mdb_strerror(err));
		return err;
	}
	return err;
}

int TXODB::GetTXOs(uint256 scriptHash, std::vector<TXO> &ntx) {
	std::lock_guard txo_lock(this->txdbMutex);
	int err = 0;
	MDB_txn *txn;
	MDB_dbi dbi;
	if (!(err = StartTXOTxn(&txn, dbi))) {
		return err;
	}

	MDB_val key{
		32,
		(unsigned char*)&scriptHash
	};

	MDB_val val;
	err = mdb_get(txn, dbi, &key, &val);
	if (err == MDB_NOTFOUND) {
		mdb_txn_abort(txn);
		return TXO_NOTFOUND; //not found, (1 is ok)
	}
	else if (err == EINVAL) {
		mdb_txn_abort(txn);
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		return err;
	}
	else {
		CDataStream ds((char*)val.mv_data, (char*)val.mv_data + val.mv_size, SER_DISK, PROTOCOL_VERSION);
		Unserialize(ds, ntx);

		mdb_txn_commit(txn);
		return TXO_OK;
	}
}

int TXODB::WriteTXOs(uint256 scriptHash, std::vector<TXO> &txns) {
	std::lock_guard txo_lock(this->txdbMutex);
	int err = 0;
	MDB_txn *txn;
	MDB_dbi dbi;
	if (!(err = StartTXOTxn(&txn, dbi))) {
		return err;
	}

	MDB_val key{ 
		32,
		(unsigned char*)&scriptHash
	};
	
	CDataStream ds(SER_DISK, PROTOCOL_VERSION);
	ds << txns;

	MDB_val val = {
		ds.size(),
		ds.data()
	};

	put_again:
	err = mdb_put(txn, dbi, &key, &val, MDB_NODUPDATA);
	if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL) {
		spdlog::error("WriteTXOs failed {}", mdb_strerror(err));
		mdb_txn_abort(txn);
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}
	else {
		mdb_txn_commit(txn);
		return TXO_OK;
	}
}

int TXODB::IncreaseMapSize() {
	int err = 0;
	struct MDB_envinfo current_info;
	if (err = mdb_env_info(this->env, &current_info)) {
		spdlog::error("Failed to get env info for resize: {}", mdb_strerror(err));
		return err;
	}

	size_t new_size = (current_info.me_mapsize - (current_info.me_mapsize % MAPSIZE_BASE_UNIT)) + MAPSIZE_BASE_UNIT;
	spdlog::info("Setting mapsize to {}", new_size);
	if (err = mdb_env_set_mapsize(this->env, new_size)) {
		spdlog::error("Failed to resize map: {}", mdb_strerror(err));
		return err;
	}
	return TXO_RESIZED;
}

int TXODB::StartTXOTxn(MDB_txn **txn, MDB_dbi& dbi) {
	int err = 0;
	if (err = mdb_txn_begin(this->env, nullptr, 0, txn)) {
		spdlog::error("Failed to start txo: {}", mdb_strerror(err));
		return err;
	}

	if (err = mdb_dbi_open(*txn, "txo", MDB_CREATE, &dbi)) {
		spdlog::error("Failed to open dbi: {}", mdb_strerror(err));
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}

	return TXO_OK;
}

int TXODB::InternalAddTXO(TXO&& nTx) {
	std::lock_guard txo_lock(this->txdbMutex);
	int err = 0;
	MDB_txn *txn;
	MDB_dbi dbi;
	if (!(err = StartTXOTxn(&txn, dbi))) {
		return err;
	}
	
	MDB_val key {
		32,
		nTx.scriptHash.begin()
	};

	MDB_val val;
	err = mdb_get(txn, dbi, &key, &val);
	if (err == EINVAL) {
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		mdb_txn_abort(txn);
		return err;
	}
	else if (err == 0 || err == MDB_NOTFOUND) {
		std::vector<TXO> vtx;
		//if data was found, load it first
		if (err == 0) {
			CDataStream ds((char*)val.mv_data, (char*)val.mv_data + val.mv_size, SER_DISK, PROTOCOL_VERSION);
			ds >> vtx;
		}
		vtx.push_back(nTx);

		CDataStream ds(SER_DISK, PROTOCOL_VERSION);
		ds << vtx;

		MDB_val val_write = {
			ds.size(),
			ds.data()
		};

		err = mdb_put(txn, dbi, &key, &val_write, MDB_NODUPDATA);
		if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL || err == EACCES) {
			spdlog::error("AddTXOs write failed {}", mdb_strerror(err));

			mdb_txn_abort(txn);
			err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
			return err;
		}
		else {
			mdb_txn_commit(txn);
			return TXO_OK;
		}
	}
	return err;
}

void TXODB::PreLoadBlocks(std::string path) {
	spdlog::info("Preload starting.. this will take some time.");
	
	mdb_env_set_flags(this->env, MDB_NOSYNC, 1);
	
	std::queue<std::filesystem::path> blks;
	std::mutex blks_pop_lock;

	std::atomic<uint32_t> nBlockFilesDone = 0;
	uint32_t nBlockFiles = 0;
	for (auto &entry : std::filesystem::directory_iterator(path)) {
		if (entry.path().filename().string().find("blk") == 0) {
			blks.push(entry.path());
		}
	}
	nBlockFiles = blks.size();

	std::vector<std::thread*> preload_threads(std::min(4u, std::thread::hardware_concurrency()));
	spdlog::info("Using {} threads for preloading..", preload_threads.size());

	std::transform(preload_threads.begin(), preload_threads.end(), preload_threads.begin(), [&blks, &blks_pop_lock, &nBlockFilesDone, nBlockFiles, this](std::thread*) {
		return new std::thread([&blks, &blks_pop_lock, &nBlockFilesDone, nBlockFiles, this] {
			std::filesystem::path blk_path;
			CBlock blk;
			while(1) {
				{
					std::lock_guard<std::mutex> pop_lock(blks_pop_lock);
					mdb_env_sync(this->env, 0);

					if(blks.size() == 0){
						break; // nothing in queue, stop thread
					}
					blk_path = blks.front();
					blks.pop();
				}

				//read block file
				auto fn = blk_path.filename().string();
				spdlog::info("Loading block file {} ({}/{} {:.2f}%)", fn, nBlockFilesDone, nBlockFiles, 100.0 * ((float)nBlockFilesDone / (float)nBlockFiles));

				auto bf_p = fopen(blk_path.string().c_str(), "rb");
				CBufferedFile bf(bf_p, PRELOAD_BUFFER_SIZE, 0, SER_DISK, PROTOCOL_VERSION);

				char net_magic[4];
				uint32_t len, offset;

				while (1) {
					memset(net_magic, 0, 4);
					len = 0;
					offset = 0;
					blk.SetNull();

					if (!feof(bf_p)) {
						try {
							bf >> net_magic;
							bf >> len;

							if (!(net_magic[0] == '\xf9' && net_magic[1] == '\xbe' && net_magic[2] == '\xb4' && net_magic[3] == '\xd9')) {
								spdlog::warn("Invalid blockchain");
								continue;
							}

							if (len > 0) {
								Unserialize(bf, blk);
								
								uint256 blkHash = blk.GetHash();
								for (auto tx : blk.vtx) {
									uint256 txHash = tx->GetHash();
									int txop = 0;

									for (auto ntxo : tx->vout) {
										uint256 sh = SHash(ntxo.scriptPubKey.begin(), ntxo.scriptPubKey.end());

										TXO t;
										t.scriptHash = sh;
										t.txHash = txHash;
										t.n = txop;
										t.value = ntxo.nValue;

										if (this->InternalAddTXO(std::move(t)) == TXO_RESIZED) {
											this->InternalAddTXO(std::move(t)); //if resized try again
										}

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
					}
					else {
						break;
					}
				}

				bf.fclose();
				nBlockFilesDone++;
			}
		});
	});

	for(auto t : preload_threads) {
		t->join();
	}

	spdlog::info("Preload finished!");
}