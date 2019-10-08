#pragma once

#include <map>
#include <vector>
#include <optional>
#include <sstream>
#include <nlohmann/json.hpp>
#include <electrumz/bitcoin/uint256.h>
#include <electrumz/bitcoin/hash.h>

namespace electrumz {
	namespace commands {
		enum ElectrumCommands {
			Unknown,
			BCBlockHeader,
			BCBlockHeaders,
			BCEstimatefee,
			BCHeadersSubscribe,
			BCRelayfee,
			SHGetBalance,
			SHGetHistory,
			SHGetMempool,
			SHHistory,
			SHListUnspent,
			SHSubscribe,
			SHUTXOS,
			TXBroadcast,
			TXGet,
			TXGetMerkle,
			TXIdFromPos,
			MPChanges,
			MPGetFeeHistogram,
			SVAddPeer,
			SVBanner,
			SVDonationAddress,
			SVFeatures,
			SVPeersSubscribe,
			SVPing,
			SVVersion
		};

		const std::map<std::string, int> CommandMap = {
			{ "blockchain.block.header", ElectrumCommands::BCBlockHeader },
			{ "blockchain.block.headers", ElectrumCommands::BCBlockHeaders },
			{ "blockchain.estimatefee", ElectrumCommands::BCEstimatefee },
			{ "blockchain.headers.subscribe", ElectrumCommands::BCHeadersSubscribe },
			{ "blockchain.relayfee", ElectrumCommands::BCRelayfee },
			{ "blockchain.scripthash.get_balance", ElectrumCommands::SHGetBalance },
			{ "blockchain.scripthash.get_history", ElectrumCommands::SHGetHistory },
			{ "blockchain.scripthash.get_mempool", ElectrumCommands::SHGetMempool },
			{ "blockchain.scripthash.history", ElectrumCommands::SHHistory },
			{ "blockchain.scripthash.listunspent", ElectrumCommands::SHListUnspent },
			{ "blockchain.scripthash.subscribe", ElectrumCommands::SHSubscribe },
			{ "blockchain.scripthash.utxos", ElectrumCommands::SHUTXOS },
			{ "blockchain.transaction.broadcast", ElectrumCommands::TXBroadcast },
			{ "blockchain.transaction.get", ElectrumCommands::TXGet },
			{ "blockchain.transaction.get_merkle", ElectrumCommands::TXGetMerkle },
			{ "blockchain.transaction.id_from_pos", ElectrumCommands::TXIdFromPos },
			{ "mempool.changes", ElectrumCommands::MPChanges },
			{ "mempool.get_fee_histogram", ElectrumCommands::MPGetFeeHistogram },
			{ "server.add_peer", ElectrumCommands::SVAddPeer },
			{ "server.banner", ElectrumCommands::SVBanner },
			{ "server.donation_address", ElectrumCommands::SVDonationAddress },
			{ "server.features", ElectrumCommands::SVFeatures },
			{ "server.peers.subscribe", ElectrumCommands::SVPeersSubscribe },
			{ "server.ping", ElectrumCommands::SVPing },
			{ "server.version", ElectrumCommands::SVVersion }
		};

		/* move these */
		class TxInfo {
		public:
			int fee;
			int height;
			std::string hash;
		};

		class TxOut {
		public:
			int pos;
			long value;
			std::string hash;
			int height;
		};

		class PeerInfo {
		public:
			std::string ip;
			std::string hostname;
			float protocol_max;
			std::optional<int> pruning_limit;
			std::optional<int> tcp_port;
			std::optional<int> ssl_port;
		};

		class ScriptStatus {
		public:
			std::vector<TxInfo> txn;

			
			uint256 GetStatusHash() const {
				std::stringstream ss;
				for (auto tx : txn) {
					ss << tx.hash << ":" << tx.height;
				}

				auto str = ss.str();
				return Hash(str.begin(), str.end());
			}
		};

		class BCBlockHeaderResponse {
		public:
			std::vector<std::string> branch;
			std::string header;
			std::string root;
		};

		class BCBlockHeadersResponse {
		public:
			int count;
			std::vector<unsigned char> hex;
			int max;
		};

		class BCEstimatefeeResponse {
		public:
			float value;
		};

		class BCHeadersSubscribeResponse {
		public:
			int height;
			std::vector<unsigned char> hex;
		};

		class BCRelayfeeResponse {
		public:
			float value;
		};

		class SHGetBalanceResponse {
		public:
			std::string confirmed;
			std::string unconfirmed;
		};

		class SHGetHistoryResponse {
		public:
			std::vector<TxInfo> history;
		};

		class SHGetMempoolResponse {
		public:
			std::vector<TxInfo> result;
		};

		class SHHistoryResponse {
		public:
			bool more;
			std::vector<std::pair<int, std::string>> history;
		};

		class SHListUnspentResponse {
		public:
			std::vector<TxOut> utxos;
		};

		class SHSubscribeResponse {
		public:
			std::string last_tx;
			ScriptStatus status;
		};

		class SHUTXOSResponse {
		public:
			bool more;
			std::vector<TxOut> utxos;
		};

		class TXBroadcastResponse {
		public:
			std::string result;
		};

		class TXGetResponse {
		public:
			//this is just the response from bitcoin core rpc call, forward it
			std::vector<unsigned char> hex_or_rpc_response;
		};

		class TXGetMerkleResponse {
		public:
			std::vector<std::string> merkle;
			int block_height;
			int pos;
		};

		class TXIdFromPosResponse {
		public:
			std::string tx_hash;
			std::vector<std::string> merkle;
		};

		class MPChangesResponse {
		public:
			//?
		};

		class MPGetFeeHistogramResponse {
		public:
			std::vector<std::pair<int, int>> history;
		};

		class SVAddPeerResponse {
		public:
			bool response;
		};

		class SVBannerResponse {
		public:
			std::string banner;
		};

		class SVDonationAddressResponse {
		public:
			std::string address;
		};

		class SVFeaturesResponse {
		public:
			std::string genesis_hash;
			std::string server_version;
			std::string hash_function;
			float protocol_max;
			float protocol_min;
			std::vector<std::tuple<std::string, int, int>> hosts;
		};

		class SVPeersSubscribeResponse {
		public:
			std::vector<PeerInfo> peers;
		};

		class SVPingResponse {

		};

		class SVVersionResponse {
		public:
			std::string software_version;
			std::string protocol_version;
		};

		template<class T>
		void to_json(nlohmann::json& j, const std::vector<T>& r);
		void to_json(nlohmann::json&, const std::string&);
		void to_json(nlohmann::json&, const PeerInfo&);
		void to_json(nlohmann::json&, const TxOut&);
		void to_json(nlohmann::json&, const ScriptStatus&);
		void to_json(nlohmann::json&, const BCBlockHeaderResponse&);
		void to_json(nlohmann::json&, const BCBlockHeadersResponse&);
		void to_json(nlohmann::json&, const BCEstimatefeeResponse&);
		void to_json(nlohmann::json&, const BCHeadersSubscribeResponse&);
		void to_json(nlohmann::json&, const BCRelayfeeResponse&);
		void to_json(nlohmann::json&, const SHGetBalanceResponse&);
		void to_json(nlohmann::json&, const SHGetHistoryResponse&);
		void to_json(nlohmann::json&, const SHGetMempoolResponse&);
		void to_json(nlohmann::json&, const SHHistoryResponse&);
		void to_json(nlohmann::json&, const SHListUnspentResponse&);
		void to_json(nlohmann::json&, const SHSubscribeResponse&);
		void to_json(nlohmann::json&, const SHUTXOSResponse&);
		void to_json(nlohmann::json&, const TXBroadcastResponse&);
		void to_json(nlohmann::json&, const TXGetResponse&);
		void to_json(nlohmann::json&, const TXGetMerkleResponse&);
		void to_json(nlohmann::json&, const TXIdFromPosResponse&);
		void to_json(nlohmann::json&, const MPChangesResponse&);
		void to_json(nlohmann::json&, const MPGetFeeHistogramResponse&);
		void to_json(nlohmann::json&, const SVAddPeerResponse&);
		void to_json(nlohmann::json&, const SVBannerResponse&);
		void to_json(nlohmann::json&, const SVDonationAddressResponse&);
		void to_json(nlohmann::json&, const SVFeaturesResponse&);
		void to_json(nlohmann::json&, const SVPeersSubscribeResponse&);
		void to_json(nlohmann::json&, const SVPingResponse&);
		void to_json(nlohmann::json&, const SVVersionResponse&);
	}
}