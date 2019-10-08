#include <electrumz/Commands.h>
#include <electrumz/bitcoin/util_strencodings.h>

#include <fmt/format.h>
#include <optional>

using namespace electrumz::commands;

void ::to_json(nlohmann::json& j, const std::string& r) {
	j = r;
}

template<class T>
void ::to_json(nlohmann::json& j, const std::vector<T>& r) {
	j = nlohmann::json{};
	for (auto x : r) {
		nlohmann::json nv;
		to_json(nv, x);
		j.push_back(nv);
	}
}

void ::to_json(nlohmann::json& j, const PeerInfo& r) {
	auto inner = nlohmann::json{
		r.protocol_max
	};

	if (r.pruning_limit.has_value()) {
		inner.push_back(fmt::format("p{0:n}", r.pruning_limit.value()));
	}
	if (r.tcp_port.has_value()) {
		inner.push_back(fmt::format("t{0:n}", r.tcp_port.value()));
	}
	if (r.ssl_port.has_value()) {
		inner.push_back(fmt::format("s{0:n}", r.ssl_port.value()));
	}
	j = nlohmann::json{
		r.ip,
		r.hostname,
		inner
	};
}

void ::to_json(nlohmann::json& j, const TxOut& r) {
	j = nlohmann::json{
		{ "tx_pos", r.pos },
		{"value", r.value},
		{"tx_hash", r.hash},
		{"height", r.height}
	};
}

void ::to_json(nlohmann::json& j, const ScriptStatus& r) {
	auto h = r.GetStatusHash();
	j = HexStr(h.begin(), h.end());
}

void ::to_json(nlohmann::json& j, const BCBlockHeaderResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const BCBlockHeadersResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const BCEstimatefeeResponse& r) {
	j = r.value;
}

void ::to_json(nlohmann::json& j, const BCHeadersSubscribeResponse& r) {
	j = nlohmann::json{
		{"height", r.height},
		{"hex", HexStr(r.hex) }
	};
}

void ::to_json(nlohmann::json& j, const BCRelayfeeResponse& r) {
	j = r.value;
}

void ::to_json(nlohmann::json& j, const SHGetBalanceResponse& r) {
	j = nlohmann::json{
		{ "confirmed", r.confirmed },
		{ "unconfiemd", r.unconfirmed }
	};
}

void ::to_json(nlohmann::json& j, const SHGetHistoryResponse& r) {
	j = nlohmann::json{ };
	for (auto h : r.history) {
		j.push_back(nlohmann::json{
			{ "height", h.height },
			{ "tx_hash", h.hash }
		});
	}
}

void ::to_json(nlohmann::json& j, const SHGetMempoolResponse& r) {
	j = nlohmann::json{ };
	for (auto h : r.result) {
		j.push_back(nlohmann::json{
			{ "height", h.height },
			{ "tx_hash", h.hash },
			{ "fee", h.fee }
		});
	}
}

void ::to_json(nlohmann::json& j, const SHHistoryResponse& r) {
	// 2.0 feature
}

void ::to_json(nlohmann::json& j, const SHListUnspentResponse& r) {
	to_json(j, r.utxos);
}

void ::to_json(nlohmann::json& j, const SHSubscribeResponse& r) {
	to_json(j, r.status);
	// 2.0 feature (return last tx hash)
}

void ::to_json(nlohmann::json& j, const SHUTXOSResponse& r) {
	// 2.0 feature
}

void ::to_json(nlohmann::json& j, const TXBroadcastResponse& r) {
	j = r.result;
}

void ::to_json(nlohmann::json& j, const TXGetResponse& r) {
	j = nlohmann::json::parse(r.hex_or_rpc_response);
	// 2.0 feature (merkle)
}

void ::to_json(nlohmann::json& j, const TXGetMerkleResponse& r) {
	j = nlohmann::json{
		{"pos", r.pos},
		{"block_height", r.block_height},
		r.merkle
	};
}

void ::to_json(nlohmann::json& j, const TXIdFromPosResponse& r) {
	j = nlohmann::json{
		{"tx_hash", r.tx_hash}
	};
}

void ::to_json(nlohmann::json& j, const MPChangesResponse& r) {
	j = nlohmann::json{ };
	// 2.0 feature
}

void ::to_json(nlohmann::json& j, const MPGetFeeHistogramResponse& r) {
	j = nlohmann::json{ };
	for (auto x : r.history) {
		j.push_back(x);
	}
}

void ::to_json(nlohmann::json& j, const SVAddPeerResponse& r) {
	j = r.response;
}

void ::to_json(nlohmann::json& j, const SVBannerResponse& r) {
	j = r.banner;
}

void ::to_json(nlohmann::json& j, const SVDonationAddressResponse& r) {
	j = r.address;
}

void ::to_json(nlohmann::json& j, const SVFeaturesResponse& r) {
	j = nlohmann::json{
		{ "genesis_hash", r.genesis_hash },
		{ "server_version", r.server_version },
		{ "hash_function", r.hash_function },
		{ "protocol_max", r.protocol_max },
		{ "protocol_min", r.protocol_min },
		{ "hosts", r.hosts }
	};
}

void ::to_json(nlohmann::json& j, const SVPeersSubscribeResponse& r) {
	j = nlohmann::json{ };
	for (auto p : r.peers) {
		nlohmann::json nv;
		to_json(nv, p);
		j.push_back(nv);
	}
}

void ::to_json(nlohmann::json& j, const SVPingResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SVVersionResponse& r) {
	j = nlohmann::json{
		{"software_version", r.software_version, },
		{"protocol_version", r.protocol_version}
	};
}