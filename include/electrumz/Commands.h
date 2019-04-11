#pragma once

#include <map>
#include <vector>
#include <optional>
#include <electrumz\CommandSerializer.h>

namespace electrumz {
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

	std::map<std::string, int> CommandMap = {
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
		int fee;
		int height;
		std::string hash;
	};

	class TxOut {
		int pos;
		long value;
		std::string hash;
		int height;
	};

	class PeerInfo {
		std::string ip;
		std::string hostname;
		float protocol_max;
		std::optional<int> pruning_limit;
		std::optional<int> tcp_port;
		std::optional<int> ssl_port;
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
		std::vector<TxInfo> result;
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
		std::vector<TxOut> result;
	};

	class SHSubscribeResponse {
	public:
		std::string last_tx;
		TxInfo info;
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
		std::vector<unsigned char> hex_or_rpc_response; //why
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
		std::string banner;
	};

	class SVDonationAddressResponse {
		std::string address;
	};

	class SVFeaturesResponse {
		std::string genesis_hash;
		std::string server_version;
		std::string hash_function;
		float protocol_max;
		float protocol_min;
		std::vector<std::tuple<std::string, int, int>> hosts;
	};

	class SVPeersSubscribeResponse {
		std::vector<PeerInfo> peers;
	};

	class SVPingResponse {

	};

	class SVVersionResponse {
		std::string software_version;
		std::string protocol_version;
	};
}