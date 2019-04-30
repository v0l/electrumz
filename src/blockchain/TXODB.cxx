#include <electrumz/TXODB.h>

#include <spdlog/spdlog.h>
#include <electrumz/bitcoin/block.h>
#include <electrumz/bitcoin/hash.h>
#include <electrumz/bitcoin/streams.h>
#include <electrumz/bitcoin/util_strencodings.h>

#include <filesystem>
#include <fstream>
#include <stdio.h>
#include <sstream>
#include <queue>
#include <atomic>
#include <chrono>

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
	if (err = mdb_env_open(this->env, this->dbPath.c_str(), MDB_CREATE | MDB_WRITEMAP, 0644)) {
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
	if (!(err = StartTXOTxn(&txn, TXO_DBI, dbi))) {
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
	if (!(err = StartTXOTxn(&txn, TXO_DBI, dbi))) {
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

int TXODB::StartTXOTxn(MDB_txn **txn, const char* dbn, MDB_dbi& dbi) {
	int err = 0;
	if (err = mdb_txn_begin(this->env, nullptr, 0, txn)) {
		spdlog::error("Failed to start txo: {}", mdb_strerror(err));
		return err;
	}

	if (err = mdb_dbi_open(*txn, dbn, MDB_CREATE, &dbi)) {
		spdlog::error("Failed to open dbi {}: {}", dbn, mdb_strerror(err));
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}

	return TXO_OK;
}

int TXODB::InputToAddr(COutPoint& output, uint256& scriptHash) {

}

int TXODB::InternalSpendTXO(COutPoint& prevout, COutPoint& spendingTx) {
	std::scoped_lock txo_lock(this->txdbMutex, this->i2aMutex);
	int err = 0;
	MDB_txn *txn;
	MDB_dbi dbi;
	MDB_dbi dbi_i2a;
	if (err = mdb_txn_begin(this->env, nullptr, 0, &txn)) {
		spdlog::error("Failed to start txo: {}", mdb_strerror(err));
		return err;
	}

	if (err = mdb_dbi_open(txn, TXO_DBI, MDB_CREATE, &dbi)) {
		spdlog::error("Failed to open dbi {}: {}", TXO_DBI, mdb_strerror(err));
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}
	
	if (err = mdb_dbi_open(txn, I2A_DBI, MDB_CREATE, &dbi_i2a)) {
		spdlog::error("Failed to open dbi {}: {}", I2A_DBI, mdb_strerror(err));
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}

	CDataStream ds(SER_DISK, PROTOCOL_VERSION);
	ds << prevout;

	MDB_val val_addr;
	MDB_val val_txns;
	MDB_val key_addr = {
		ds.size(),
		ds.data()
	};

	err = mdb_get(txn, dbi_i2a, &key_addr, &val_addr);
	if(err != 0){
		spdlog::error("UTXO database is corrupt (COutPoint {}:{} not found): {}", prevout.hash.GetHex(), prevout.n, mdb_strerror(err));
		spdlog::error("Key hex: {}", HexStr((char*)key_addr.mv_data, (char*)key_addr.mv_data + key_addr.mv_size));
		mdb_txn_abort(txn);
		return err;
	}

	ds.clear();
	err = mdb_get(txn, dbi, &val_addr, &val_txns);
	if(err != 0){
		spdlog::error("UTXO database is corrupt (Address not found): {}", mdb_strerror(err));
		mdb_txn_abort(txn);
		return err;
	}
	
	//de-serialize txns
	ds.write((char*)val_txns.mv_data, val_txns.mv_size);
	std::vector<TXO> txns;
	ds >> txns;

	bool txoFound = false;
	for(auto& txo : txns){
		if(txo.txHash == prevout.hash && txo.n == prevout.n){
			txoFound = true;
			txo.spendingTxi.hash = spendingTx.hash;
			txo.spendingTxi.n = spendingTx.n;
			break;
		}
	}

	if(!txoFound) {
		spdlog::error("UTXO database is corrupt (Output not found for address): {}", mdb_strerror(err));
		mdb_txn_abort(txn);
		return err;
	}

	//write back updated txos
	ds.clear();
	ds << txns;

	val_txns.mv_data = ds.data();
	val_txns.mv_size = ds.size();

	err = mdb_put(txn, dbi, &val_addr, &val_txns, MDB_NODUPDATA);
	if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL || err == EACCES) {
		spdlog::error("InternalSpendTXO write failed {}", mdb_strerror(err));

		mdb_txn_abort(txn);
		err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
		return err;
	}

	mdb_txn_commit(txn);
	return TXO_OK;
}

int TXODB::InternalAddTXO(TXO& nTx, MDB_txn* txn, MDB_dbi& dbi, MDB_dbi& dbi_i2a) {
	//std::scoped_lock txo_lock(this->txdbMutex, this->i2aMutex);
	int err = 0;
	
	MDB_val key {
		32,
		nTx.scriptHash.begin()
	};

	MDB_val val;
	err = mdb_get(txn, dbi, &key, &val);
	if (err != 0 && err != MDB_NOTFOUND) {
		spdlog::error("Error getting txos {}", mdb_strerror(err));
		return err;
	}
	else {
		CDataStream ds(SER_DISK, PROTOCOL_VERSION);
		std::vector<TXO> vtx;
		//if data was found, load it first
		if (err == 0) {
			ds.write((char*)val.mv_data, val.mv_size);
			ds >> vtx;
		}
		vtx.push_back(nTx);
		ds.clear();

		ds << vtx;
		
		MDB_val val_write = {
			ds.size(),
			ds.data()
		};

		err = mdb_put(txn, dbi, &key, &val_write, NULL);
		if (err != 0) {
			spdlog::error("AddTXOs write failed {}", mdb_strerror(err));
			if(err == MDB_MAP_FULL) {
				mdb_txn_abort(txn);
				err = this->IncreaseMapSize();
			} 
			return err;
		}
		
		//track new outputs to their address
		//COutPoint -> ScriptHash
		ds.clear();
		COutPoint nout;
		nout.hash = nTx.txHash;
		nout.n = nTx.n;

		ds << nout;
		MDB_val key_i2a = {
			ds.size(),
			ds.data()
		};

		err = mdb_put(txn, dbi_i2a, &key_i2a, &key, NULL);
		if (err != 0) {
			spdlog::error("AddTXOs(i2a) write failed {}", mdb_strerror(err));
			if(err == MDB_MAP_FULL) {
				mdb_txn_abort(txn);
				err = this->IncreaseMapSize();
			} 
			return err;
		}

		return TXO_OK;
	}
	return err;
}


void TXODB::PreLoadBlocks(std::string path) {
	spdlog::info("Preload starting.. this will take some time.");
	
	//mdb_env_set_flags(this->env, MDB_NOSYNC, 1);

	std::vector<std::filesystem::path> blks;
	std::mutex blks_pop_lock;

	std::atomic<uint32_t> nBlockFilesDone = 0;
	uint32_t nBlockFiles = 0;
	for (auto &entry : std::filesystem::directory_iterator(path)) {
		if (entry.path().filename().string().find("blk") == 0) {
			blks.push_back(entry.path());
			nBlockFiles++;
			if(nBlockFiles > 0){
				break;
			}
		}
	}

	auto pos_txo = blks.begin();
	auto pos_txo_end = blks.end();
	std::vector<std::thread*> preload_threads(std::min(1u, std::thread::hardware_concurrency()));
	spdlog::info("Using {} threads for preloading..", preload_threads.size());
	
	int mode = 0;
	auto preload_func = [&mode, &pos_txo, &pos_txo_end, &blks_pop_lock, &nBlockFilesDone, nBlockFiles, this] {
		std::filesystem::path blk_path;
		CBlock blk;
		while(1) {
			{
				std::lock_guard<std::mutex> pos_lock(blks_pop_lock);
				//mdb_env_sync(this->env, 0);

				if(pos_txo == pos_txo_end){
					break; // end of block list
				}
				blk_path = *pos_txo;
				nBlockFilesDone++;

				std::advance(pos_txo, 1);
			}

			//read block file
			auto fn = blk_path.filename().string();
			spdlog::info("Loading block file {} ({}/{} {:.2f}%)", fn, nBlockFilesDone, nBlockFiles, 100.0 * ((float)nBlockFilesDone / (float)nBlockFiles));

			auto bf_p = fopen(blk_path.string().c_str(), "rb");
			CBufferedFile bf(bf_p, PRELOAD_BUFFER_SIZE, 0, SER_DISK, PROTOCOL_VERSION);

			char net_magic[4];
			uint32_t len, offset;
			uint32_t nBlocks;
			
			auto start = std::chrono::system_clock::now();
			while (1) {
				memset(net_magic, 0, 4);
				len = 0;
				offset = 0;
				blk.SetNull();

				if (!bf.eof()) {
					try {
						bf >> net_magic;
						bf >> len;

						if (!(net_magic[0] == '\xf9' && net_magic[1] == '\xbe' && net_magic[2] == '\xb4' && net_magic[3] == '\xd9')) {
							spdlog::warn("Invalid blockchain");
							continue;
						}

						if (len > 0) {
							Unserialize(bf, blk);
							
							//start a tx for this block
							int err = 0;
							MDB_txn *txn;
							MDB_dbi dbi;
							MDB_dbi dbi_i2a;

							try_block_again:
							if (err = mdb_txn_begin(this->env, nullptr, 0, &txn)) {
								spdlog::error("Failed to start txo: {}", mdb_strerror(err));
								return err;
							}

							if (err = mdb_dbi_open(txn, TXO_DBI, MDB_CREATE, &dbi)) {
								spdlog::error("Failed to open dbi {}: {}", TXO_DBI, mdb_strerror(err));
								err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
								return err;
							}
							
							if (err = mdb_dbi_open(txn, I2A_DBI, MDB_CREATE, &dbi_i2a)) {
								spdlog::error("Failed to open dbi {}: {}", I2A_DBI, mdb_strerror(err));
								err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
								return err;
							}

							uint256 blkHash = blk.GetHash();
							for (auto tx : blk.vtx) {
								uint256 txHash = tx->GetHash();
								
								//process inputs or outputs
								if(mode == 1){
									int txip = 0;
									for (auto ntxi : tx->vin){
										//check is coinbase txin
										if(!ntxi.prevout.IsNull()){
											COutPoint thisTxi;
											thisTxi.hash = txHash;
											thisTxi.n = txip;
											if(this->InternalSpendTXO(ntxi.prevout, thisTxi) == TXO_RESIZED){
												this->InternalSpendTXO(ntxi.prevout, thisTxi);
											}
										}

										txip++;
									}
								} else {
									int txop = 0;
									for (auto ntxo : tx->vout) {
										uint256 sh = SHash(ntxo.scriptPubKey.begin(), ntxo.scriptPubKey.end());

										TXO t;
										t.scriptHash = sh;
										t.txHash = txHash;
										t.n = txop;
										t.value = ntxo.nValue;
										t.spendingTxi.SetNull();

										if ((err = this->InternalAddTXO(t, txn, dbi, dbi_i2a)) != TXO_OK) {
											if(err != TXO_RESIZED){ //in this case the tx is already aborted
												mdb_txn_abort(txn);
											}
											spdlog::info("GOTO RETRY");
											goto try_block_again;
										}

										txop++;
									}
								}
							}

							mdb_txn_commit(txn);
							nBlocks++;

							std::chrono::duration<double> nt = std::chrono::system_clock::now() - start;
							if(nt.count() >= 5){
								start = std::chrono::system_clock::now();
								spdlog::info("{} blocks in {:.2f}s - {:.2f} blk/s", nBlocks, nt.count(), nBlocks / nt.count());
								nBlocks = 0;
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
		}
	};

	//load all outputs first
	std::transform(preload_threads.begin(), preload_threads.end(), preload_threads.begin(), [&](std::thread*) {
		return new std::thread(preload_func);
	});
	for(auto t : preload_threads) {
		t->join();
	}

	spdlog::info("All outputs are loaded!");

	//spend 
	//mdb_env_sync(this->env, 1);
	nBlockFilesDone = 0;
	mode = 1;
	pos_txo = blks.begin();
	std::transform(preload_threads.begin(), preload_threads.end(), preload_threads.begin(), [&](std::thread*) {
		return new std::thread(preload_func);
	});
	for(auto t : preload_threads) {
		t->join();
	}

	spdlog::info("Preload finished!");
}