#pragma once

#include <nlohmann\json.hpp>
#include <electrumz\Commands.h>

namespace electrumz {
	void to_json(nlohmann::json& j, const BCBlockHeaderResponse& r) {
		j = nlohmann::json{ {"branch", r.branch } };
	}

	void to_json(nlohmann::json& j, const BCBlockHeadersResponse& r) {

	}

	void to_json(nlohmann::json& j, const BCEstimatefeeResponse& r) {

	}

	void to_json(nlohmann::json& j, const BCHeadersSubscribeResponse& r) {

	}

	void to_json(nlohmann::json& j, const BCRelayfeeResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHGetBalanceResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHGetHistoryResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHGetMempoolResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHHistoryResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHListUnspentResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHSubscribeResponse& r) {

	}

	void to_json(nlohmann::json& j, const SHUTXOSResponse& r) {

	}

	void to_json(nlohmann::json& j, const TXBroadcastResponse& r) {

	}

	void to_json(nlohmann::json& j, const TXGetResponse& r) {

	}

	void to_json(nlohmann::json& j, const TXGetMerkleResponse& r) {

	}

	void to_json(nlohmann::json& j, const TXIdFromPosResponse& r) {

	}

	void to_json(nlohmann::json& j, const MPChangesResponse& r) {

	}

	void to_json(nlohmann::json& j, const MPGetFeeHistogramResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVAddPeerResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVBannerResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVDonationAddressResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVFeaturesResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVPeersSubscribeResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVPingResponse& r) {

	}

	void to_json(nlohmann::json& j, const SVVersionResponse& r) {

	}
}