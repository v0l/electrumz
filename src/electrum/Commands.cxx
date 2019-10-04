#include <electrumz/Commands.h>
#include <electrumz/bitcoin/util_strencodings.h>

using namespace electrumz::commands;

void ::to_json(nlohmann::json& j, const BCBlockHeaderResponse& r) {
	j = nlohmann::json{
		{ "branch", r.branch }
	};
}

void ::to_json(nlohmann::json& j, const BCBlockHeadersResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const BCEstimatefeeResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const BCHeadersSubscribeResponse& r) {
	j = nlohmann::json{
		{"height", r.height},
		{"hex", HexStr(r.hex) }
	};
}

void ::to_json(nlohmann::json& j, const BCRelayfeeResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHGetBalanceResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHGetHistoryResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHGetMempoolResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHHistoryResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHListUnspentResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHSubscribeResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SHUTXOSResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const TXBroadcastResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const TXGetResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const TXGetMerkleResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const TXIdFromPosResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const MPChangesResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const MPGetFeeHistogramResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SVAddPeerResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SVBannerResponse& r) {
	j = nlohmann::json{ };
}

void ::to_json(nlohmann::json& j, const SVDonationAddressResponse& r) {
	j = nlohmann::json{ };
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