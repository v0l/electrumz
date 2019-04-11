#pragma once

#include <map>
#include <functional>
#include <uv.h>

#include <nlohmann\json.hpp>
#ifndef ELECTRUMZ_NO_SSL
#include <mbedtls\ssl.h>
#endif

#include <electrumz\Commands.h>

namespace electrumz {
	namespace net {
		enum JsonRPCState {
			UNKNOWN = 0,
			START = 1, //flag for first read from socket
			NORMAL = 2, //flag for non-ssl connection
#ifndef ELECTRUMZ_NO_SSL
			SSL_NORMAL = 4, //flag for ssl connection
			SSL_HANDSHAKE = 8 //flag for ssl handshaking in progress
#endif
		};

		class JsonRPCServer {
		public:
#ifndef ELECTRUMZ_NO_SSL
			JsonRPCServer(uv_tcp_t*, mbedtls_ssl_config*);
#else
			JsonRPCServer(uv_tcp_t*);
#endif

			int Write(ssize_t, unsigned char*);
			void End();
		private:
			int HandleRead(ssize_t, const uv_buf_t*);
			int HandleWrite(uv_write_t*, int);
			int AppendBuffer(ssize_t, unsigned char*);
			int WriteInternal(ssize_t, unsigned char*);
			bool IsTLSClientHello(ssize_t, char*);
			int HandleCommand(nlohmann::json&);

#ifndef ELECTRUMZ_NO_SSL
			void InitTLSContext();
			int TryHandshake();
			mbedtls_ssl_context *ssl;
			mbedtls_ssl_config *ssl_config;

			//We set this on recv and read it from our bio cbs
			char* ssl_buf = nullptr;
			ssize_t ssl_buf_offset = 0;
			ssize_t ssl_buf_len = 0;
#endif

			//the connection
			uv_tcp_t *stream;
			int state;

			unsigned char *buf = nullptr;
			ssize_t offset = 0;
			ssize_t len = 0;

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
				{ "server.version", ElectrumCommands::SVVersion },
				{ "masternode.announce.broadcast", ElectrumCommands::MNAnnounceBroadcast },
				{ "masternode.subscribe", ElectrumCommands::MNSubscribe },
				{ "masternode.list", ElectrumCommands::MNList },
				{ "protx.diff", ElectrumCommands::ProtxDiff },
				{ "protx.info", ElectrumCommands::ProtxInfo }
			};
		};
	}
}