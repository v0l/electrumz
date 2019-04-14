#include <electrumz/TXODB.h>

#include <spdlog/spdlog.h>
#include <electrumz/bitcoin/block.h>
#include <electrumz/bitcoin/streams.h>
#include <electrumz/bitcoin/key_io.h>

#include <filesystem>
#include <stdio.h>
#include <sstream>

using namespace electrumz::blockchain;

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
	if (err = mdb_env_open(this->env, path.c_str(), MDB_NOTLS | MDB_CREATE | MDB_WRITEMAP | MDB_MAPASYNC, NULL)) {
		spdlog::error("mdb open failed: {}", mdb_strerror(err));
		return;
	}
}

int TXODB::GetTx(MDB_txn **tx) {
	return mdb_txn_begin(this->env, NULL, 0, tx);
}

int TXODB::GetTXOs(MDB_txn* txn, uint256 scriptHash, std::vector<TXO> &txos) {
	std::lock_guard _(txdbMutex);
	MDB_dbi dbi;
	int err = mdb_dbi_open(txn, "txo", MDB_CREATE, &dbi);

	MDB_val key{ scriptHash.size(), scriptHash.begin() };
	MDB_val val;
	err = mdb_get(txn, dbi, &key, &val);
	if (err == MDB_NOTFOUND) {
		mdb_dbi_close(this->env, dbi);
		return 2; //not found, (1 is ok)
	}
	else if (err == EINVAL) {
		return 0;
	}
	else {
		CDataStream ds((char*)val.mv_data, (char*)val.mv_data + val.mv_size, SER_DISK, PROTOCOL_VERSION);
		ds >> txos;

		mdb_dbi_close(this->env, dbi);
		return 1;
	}
}

int TXODB::AddTXO(TXO nTx) {
	MDB_txn *txn;
	mdb_txn_begin(this->env, nullptr, 0, &txn);

	std::vector<TXO> txos;
	if (!this->GetTXOs(txn, nTx.scriptHash, txos)) {
		spdlog::error("error getting txos");
		return 0;
	}

	txos.push_back(nTx);
	return this->WriteTXOs(txn, nTx.scriptHash, txos);
}

int TXODB::WriteTXOs(MDB_txn *txn, uint256 scriptHash, std::vector<TXO> txns) {
	std::lock_guard _(txdbMutex);
	MDB_dbi dbi;
	int err = mdb_dbi_open(txn, "txo", MDB_CREATE, &dbi);

	MDB_val key{ scriptHash.size(), scriptHash.begin() };
	CDataStream ds(SER_DISK, PROTOCOL_VERSION, txns);
	
	std::stringstream ss;
	Serialize(ss, txns);

	MDB_val val;
	std::string out = ss.str();
	val.mv_data = (void*)out.c_str();
	val.mv_size = out.size();

	err = mdb_put(txn, dbi, &key, &val, MDB_NODUPDATA);
	if (err == MDB_MAP_FULL || err == MDB_TXN_FULL || err == EINVAL) {
		mdb_dbi_close(this->env, dbi);
		return 0;
	}
	else {
		mdb_dbi_close(this->env, dbi);
		return 1;
	}
}

void TXODB::PreLoadBlocks(std::string path) {
	spdlog::info("Preload starting.. this will take some time.");

	for (const auto & entry : std::filesystem::directory_iterator(path)) {
		auto fn = entry.path().filename().string();
		if (fn.find("blk") == 0) {
			spdlog::info("Loading block file {}", fn);

			auto bf_p = fopen(entry.path().string().c_str(), "rb");
			CAutoFile bf(bf_p, SER_DISK, PROTOCOL_VERSION);
			
			//this is not the actual hight, since blocks are not in order
			//but its approx for reporting purposes
			uint32_t height = 0; 
			CBlock blk;
			char net_magic[4];
			uint32_t len;

			while (1) {
				blk.SetNull();

				if (feof(bf.Get())) {
					break;
				}
				try {
					bf >> net_magic;
					bf >> len;

					if (len > 0) {
						bf >> blk;
						
						
						std::vector<CTxDestination> dest;
						for (auto tx : blk.vtx) {
							auto txHash = tx->GetHash();
							int txop = 0;
							int nreq = 0;
							txnouttype ot;

							for (auto txo : tx->vout) {

								/* not needed for electum ?
								if (ExtractDestinations(txo.scriptPubKey, ot, dest, nreq)) {
									for (auto dest_out : dest) {
										std::string addr = EncodeDestination(dest_out);

										
									}
								}*/

								uint256 h = SHash(txo.scriptPubKey.begin(), txo.scriptPubKey.end());
								this->AddTXO(TXO(h, txHash, txop, txo.nValue));

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
					break;
				}
			}

		}
	}
	spdlog::info("Preload finished!");
}