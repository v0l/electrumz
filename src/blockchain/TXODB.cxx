#include <electrumz/TXODB.h>

#include <lmdb.h>
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

constexpr size_t MAPSIZE_BASE_UNIT = 1024 * 1024 * 100;
#define PRELOAD_BUFFER_SIZE 0x8000000 //load full blockfiles in RAM, this is called "MAX_BLOCKFILE_SIZE" in Bitcoin Core

TXODB::TXODB(std::string path) {
	this->dbPath = path;
}

int TXODB::Open() {
	int err = 0;
	if (err = mdb_env_create(&this->env)) {
		spdlog::error("mdb create failed {}", mdb_strerror(err));
		return err;
	}
	if (err = mdb_env_set_maxdbs(this->env, 3)) {
		spdlog::error("Failed to set max dbs: {}", mdb_strerror(err));
		return err;
	}
	if (err = mdb_env_open(this->env, this->dbPath.c_str(), MDB_CREATE | MDB_WRITEMAP | MDB_MAPASYNC | MDB_NOSUBDIR, 0644)) {
		spdlog::error("mdb open failed: {}", mdb_strerror(err));
		return err;
	}

	struct MDB_envinfo current_info;
	if (err = mdb_env_info(this->env, &current_info)) {
		spdlog::error("Failed to get env info for resize: {}", mdb_strerror(err));
		return err;
	}

	if (current_info.me_mapsize < MAPSIZE_BASE_UNIT) {
		size_t new_size = MAPSIZE_BASE_UNIT;
		spdlog::info("Setting mapsize to {}", new_size);
		if (err = mdb_env_set_mapsize(this->env, new_size)) {
			spdlog::error("Failed to resize map: {}", mdb_strerror(err));
			return err;
		}
	}

	return err;
}

int TXODB::GetTXOStats(MDB_stat* stats, const char* dbn) {
	int err = 0;
	MDB_txn* txn;
	MDB_dbi dbi;
	if (!(err = StartTXOTxn(&txn, dbn, dbi))) {
		return err;
	}

	if ((err = mdb_stat(txn, dbi, stats))) {
		return err;
	}

	mdb_txn_commit(txn);
	return err;
}

int TXODB::GetTXOs(uint256 scriptHash, std::vector<TXO>& ntx) {
	int err = 0;
	MDB_txn* txn;
	MDB_dbi dbi;
	if (!(err = StartTXOTxn(&txn, DBI_ADDR, dbi))) {
		return err;
	}

	MDB_val key{
		scriptHash.size(),
		scriptHash.begin()
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
		COutPoint outPoint;
		spdlog::debug("Got val for {}: {}", HexStr(scriptHash), HexStr((char*)val.mv_data, (char*)val.mv_data + val.mv_size));

		CDataStream ds((char*)val.mv_data, (char*)val.mv_data + val.mv_size, SER_DISK, PROTOCOL_VERSION);
		outPoint.Unserialize(ds);

		mdb_txn_commit(txn);
		return TXO_OK;
	}
}

int TXODB::IncreaseMapSize() {
	std::lock_guard<std::mutex> x(this->resize_lock);
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

int TXODB::StartTXOTxn(MDB_txn** txn, const char* dbn, MDB_dbi& dbi) {
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


int TXODB::InternalSpendTXO(COutPoint& prevout, COutPoint& spendingTx) {
	//std::scoped_lock txo_lock(this->txdbMutex, this->i2aMutex);
	/*int err = 0;
	MDB_txn *txn;
	MDB_dbi dbi;
	if (err = mdb_txn_begin(this->env, nullptr, 0, &txn)) {
		spdlog::error("Failed to start txo: {}", mdb_strerror(err));
		return err;
	}

	if (err = mdb_dbi_open(txn, TXO_DBI, MDB_CREATE, &dbi)) {
		spdlog::error("Failed to open dbi {}: {}", TXO_DBI, mdb_strerror(err));
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

	mdb_txn_commit(txn);*/
	return TXO_OK;
}

int TXODB::InternalAddTXO(TXO& nTx, MDB_txn* txn, MDB_dbi& dbi, MDB_dbi& dbi_i2a) {
	//std::scoped_lock txo_lock(this->txdbMutex, this->i2aMutex);
	int err = 0;

	//output point to store for this address
	COutPoint op;
	op.hash = nTx.txHash;
	op.n = nTx.n;

	CDataStream ds(SER_DISK, PROTOCOL_VERSION);
	ds << op;

	//scriptHash(address) key
	MDB_val addr_key{
		nTx.scriptHash.size(),
		nTx.scriptHash.begin()
	};
	MDB_val addr_val = {
		ds.size(),
		ds.data()
	};

	err = mdb_put(txn, dbi, &addr_key, &addr_val, NULL);
	if (err != 0) {
		spdlog::error("AddTXOs write failed {}", mdb_strerror(err));
		if (err == MDB_MAP_FULL) {
			mdb_txn_abort(txn);
			err = this->IncreaseMapSize();
		}
		return err;
	}

	//clear the datastream and store the txo
	ds.clear();
	ds << nTx;

	//this should probably be in a seperate db
	//but instead we rely on no hash collisions for sha256
	//txHash <> scriptHash could collide
	MDB_val tx_key = {
		nTx.txHash.size(),
		nTx.txHash.begin()
	};
	MDB_val tx_val = {
		ds.size(),
		ds.data()
	};

	err = mdb_put(txn, dbi, &tx_key, &tx_val, NULL);
	if (err != 0) {
		spdlog::error("AddTXO write failed {}", mdb_strerror(err));
		if (err == MDB_MAP_FULL) {
			mdb_txn_abort(txn);
			err = this->IncreaseMapSize();
		}
		return err;
	}

	return TXO_OK;
}

/// Pushes block tip with
int TXODB::PushBlockTip(MDB_txn* tx, const CBlockHeader& h) {
	return TXO_OK;
}

void TXODB::PreLoadBlocks(std::string path) {
	spdlog::info("Preload starting.. this will take some time.");

	mdb_env_set_flags(this->env, MDB_NOSYNC, 1);

	std::vector<std::filesystem::path> blks;
	std::mutex blks_pop_lock;

	std::atomic<uint32_t> nBlockFilesDone = 0;
	std::atomic<uint64_t> rate_block_process = 0;
	std::atomic<uint64_t> rate_tx_process = 0;
	std::atomic<uint64_t> total_tx_process = 0;
	std::atomic<uint64_t> total_block_process = 0;
	std::atomic<std::chrono::time_point<std::chrono::system_clock>> rate_last_print = std::chrono::system_clock::now();

	uint32_t nBlockFiles = 0;
	for (auto& entry : std::filesystem::directory_iterator(path)) {
		if (entry.path().filename().string().find("blk") == 0) {
			blks.push_back(entry.path());
			nBlockFiles++;
		}
	}

	auto pos_txo = blks.begin();
	auto pos_txo_end = blks.end();
	std::vector<std::thread*> preload_threads(std::min(4u, std::thread::hardware_concurrency()));
	spdlog::info("Using {} threads for preloading..", preload_threads.size());

	int mode = 0;
	auto preload_func = [&mode, &pos_txo, &pos_txo_end, &blks_pop_lock, &nBlockFilesDone, nBlockFiles, this, &rate_tx_process, &rate_last_print, &total_tx_process, &rate_block_process, &total_block_process] {
		std::filesystem::path blk_path;
		CBlock blk;
		while (1) {
			{
				std::lock_guard<std::mutex> pos_lock(blks_pop_lock);
				//mdb_env_sync(this->env, 0);

				if (pos_txo == pos_txo_end) {
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
			CBufferedFile bf(bf_p, PRELOAD_BUFFER_SIZE, 4, SER_DISK, PROTOCOL_VERSION); //allow rewinding of net_magic

			char net_magic[4];
			uint32_t len = 0, offset = 0;

			while (1) {
				memset(net_magic, 0, 4);
				len = 0;
				offset = 0;
				blk.SetNull();

				if (!bf.eof()) {
					try {
						bf >> net_magic;
						bf >> len;

						if (len == 0) {
							break;
						}

						if (!(net_magic[0] == '\xfa' && net_magic[1] == '\xbf' && net_magic[2] == '\xb5' && net_magic[3] == '\xda')) {
							spdlog::warn("Invalid blockchain {}", len);
							continue;
						}

						if (len > 0) {
							//peek and see if we have a missing block
							bf >> net_magic;
							bf.SetPos(bf.GetPos() - 4);
							if (net_magic[0] == '\xfa' && net_magic[1] == '\xbf' && net_magic[2] == '\xb5' && net_magic[3] == '\xda') {
								//we haved an empty block go skip to next read
								continue;
							}

							bf >> blk;

							//start a tx for this block
							int err = 0;
							MDB_txn* txn;
							MDB_dbi dbi;
							MDB_dbi dbi_addr;
							MDB_dbi dbi_blk;

						try_block_again:
							if (err = mdb_txn_begin(this->env, nullptr, 0, &txn)) {
								spdlog::error("Failed to start txo: {}", mdb_strerror(err));
								return err;
							}

							if (err = mdb_dbi_open(txn, DBI_TXO, MDB_CREATE | MDB_DUPSORT, &dbi)) {
								spdlog::error("Failed to open dbi {}: {}", DBI_TXO, mdb_strerror(err));
								err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
								return err;
							}

							if (err = mdb_dbi_open(txn, DBI_ADDR, MDB_CREATE | MDB_DUPSORT, &dbi_addr)) {
								spdlog::error("Failed to open dbi {}: {}", DBI_ADDR, mdb_strerror(err));
								err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
								return err;
							}
							if (err = mdb_dbi_open(txn, DBI_BLK, MDB_CREATE, &dbi_blk)) {
								spdlog::error("Failed to open dbi {}: {}", DBI_BLK, mdb_strerror(err));
								err == MDB_MAP_FULL && (err = this->IncreaseMapSize());
								return err;
							}

							// Add dupsort for txo
							if (err = mdb_set_dupsort(txn, dbi, [](const MDB_val* a, const MDB_val* b) -> int {
								//first member of a TXO is its N index, use this to sort the outputs
								uint32_t* an = (uint32_t*)a->mv_data;
								uint32_t* bn = (uint32_t*)b->mv_data;
								return *an - *bn;
								})) {
								spdlog::error("Failed to set cmpfunc for dbi {}: {}", DBI_TXO, mdb_strerror(err));
								return err;
							}

							CDataStream ds(SER_DISK, PROTOCOL_VERSION);
							uint256 blkHash = blk.GetHash();

							//Store the block header
							MDB_val blk_key = {
									blkHash.size(),
									blkHash.begin()
							};

							ds.clear();
							ds.reserve(80);
							ds << blk.GetBlockHeader();

							MDB_val blk_val = {
								ds.size(),
								ds.data()
							};

							err = mdb_put(txn, dbi_blk, &blk_key, &blk_val, MDB_NODUPDATA);
							if (err != 0) {
								spdlog::error("AddBLK write failed {}", mdb_strerror(err));
								if (err == MDB_MAP_FULL) {
									mdb_txn_abort(txn);
									if ((err = this->IncreaseMapSize()) != TXO_RESIZED) {
										spdlog::error("Failed to increase map size, thread exiting..");
										return err;
									}
									goto try_block_again;
								}
								return err;
							}

							for (auto tx : blk.vtx) {
								uint256 txHash = tx->GetHash();

								MDB_cursor* curtx;
								mdb_cursor_open(txn, dbi, &curtx);

								//create a reference to this transaction as an output
								COutPoint out_tx;
								out_tx.hash = txHash;

								MDB_val tx_key = {
									txHash.size(),
									txHash.begin()
								};

								//process inputs or outputs
								if (mode == 1) {
									int txip = 0;
									for (auto ntxi : tx->vin) {
										//check is coinbase txin
										if (!ntxi.prevout.IsNull()) {
											COutPoint thisTxi;
											thisTxi.hash = txHash;
											thisTxi.n = txip;
											if (this->InternalSpendTXO(ntxi.prevout, thisTxi) == TXO_RESIZED) {
												this->InternalSpendTXO(ntxi.prevout, thisTxi);
											}
										}

										txip++;
									}
								}
								else {
									int txop = 0;
									for (auto ntxo : tx->vout) {
										uint256 sh = SHash(ntxo.scriptPubKey.begin(), ntxo.scriptPubKey.end());

										TXO t(sh, txHash, txop++, ntxo.nValue, 0);
										out_tx.n = t.n; //output n

										int err = 0;
										ds.clear();
										ds.reserve(36);
										ds << out_tx;

										//scriptHash(address) key
										MDB_val addr_key{
											sh.size(),
											sh.begin()
										};
										MDB_val addr_val = {
											ds.size(),
											ds.data()
										};

										err = mdb_put(txn, dbi_addr, &addr_key, &addr_val, MDB_NODUPDATA);
										if (err != 0) {
											if (err == MDB_MAP_FULL) {
												mdb_txn_abort(txn);
												if ((err = this->IncreaseMapSize()) != TXO_RESIZED) {
													spdlog::error("Failed to increase map size, thread exiting..");
													return err;
												}
												goto try_block_again;
											}
											else if (err == MDB_KEYEXIST) {
												spdlog::warn("Duplicate output for addr {} ({}:{})", sh.GetHex(), out_tx.hash.GetHex(), out_tx.n);
												continue;
											}

											spdlog::error("Addr txns write failed {}", mdb_strerror(err));
											return err;
										}
										else {
											spdlog::debug("Found new scriptHash: {}", HexStr(sh));
											spdlog::debug("=val: {}", HexStr((char*)addr_val.mv_data, (char*)addr_val.mv_data + addr_val.mv_size));
										}

										//clear the datastream and store the txo
										ds.clear();
										ds.reserve(TXO::ApproxSize);
										ds << t;

										MDB_val tx_val = {
											ds.size(),
											ds.data()
										};

										err = mdb_cursor_put(curtx, &tx_key, &tx_val, MDB_APPENDDUP);
										if (err != 0) {
											spdlog::error("AddTXO write failed {}", mdb_strerror(err));
											if (err == MDB_MAP_FULL) {
												mdb_txn_abort(txn);
												if ((err = this->IncreaseMapSize()) != TXO_RESIZED) {
													spdlog::error("Failed to increase map size, thread exiting..");
													return err;
												}
												goto try_block_again;
											}
											return err;
										}
									}
								}

								mdb_cursor_close(curtx);
								rate_tx_process++;
								total_tx_process++;
							}

							mdb_txn_commit(txn);
							rate_block_process++;
							total_block_process++;

							auto last_print = rate_last_print.load();
							std::chrono::duration<double> nt = std::chrono::system_clock::now() - last_print; //race
							if (nt.count() >= 5) {
								auto last_interval = rate_last_print.exchange(std::chrono::system_clock::now());
								//check again
								if ((std::chrono::system_clock::now() - last_interval).count() >= 5) {
									auto ntx = rate_tx_process.exchange(0);
									auto nblk = rate_block_process.exchange(0);
									auto ttx = total_tx_process.load();
									auto tblk = total_block_process.load();
									spdlog::info("{:n} blk/s, {:n} txo/s, {:n} txn, {:n} blk", (uint64_t)(nblk / nt.count()), (uint64_t)(ntx / nt.count()), ttx, tblk);
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
			mdb_env_sync(this->env, 1);//make sure to force sync here!
		}
	};

	//load all outputs first
	std::transform(preload_threads.begin(), preload_threads.end(), preload_threads.begin(), [&](std::thread*) {
		return new std::thread(preload_func);
		});
	for (auto t : preload_threads) {
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
	for (auto t : preload_threads) {
		t->join();
	}

	spdlog::info("Preload finished!");
}