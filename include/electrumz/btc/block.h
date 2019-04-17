#pragma once

#include <electrumz/btc/tx.h>
#include <electrumz/btc/def.h>

namespace electrumz {
	namespace bitcoin {
		class BlockHeader : Serializable {
		private:
		public:
			BlockHeader() { this->SetNull(); }

			int32_t nVersion;
			sha256 hashPrevBlock;
			sha256 hashMerkleRoot;
			uint32_t nTime;
			uint32_t nBits;
			uint32_t nNonce;

			ADD_SERIALABLE_FUNC_DEF;
			sha256 GetHash();
		};

		class Block : Serializable {
		public:
			Block() { this->SetNull(); }

			BlockHeader header;
			std::vector<Tx> vtx;

			ADD_SERIALABLE_FUNC_DEF;
			sha256 GetHash() { return header.GetHash(); }
		};
	}
}