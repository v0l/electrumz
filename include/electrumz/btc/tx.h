#pragma once 

#include <electrumz/btc/def.h>
#include <stdint.h>
#include <vector>

namespace electrumz {
	namespace bitcoin {
		typedef std::vector<unsigned char> Script;
		typedef std::vector<Script> WitnessScript;

		class OutPoint : Serializable {
		public:
			sha256 hash;
			uint32_t n;

			ADD_SERIALABLE_FUNC_DEF;
		};

		class TxIn : Serializable {
		public:
			OutPoint prevout;
			Script scriptSig;
			uint32_t nSequence;
			WitnessScript scriptWitness;

			ADD_SERIALABLE_FUNC_DEF;
		};

		class TxOut : Serializable {
		public:
			int64_t nValue;
			Script scriptPubKey;

			ADD_SERIALABLE_FUNC_DEF;
		};

		class Tx : Serializable {
		public:
			int32_t nVersion;
			std::vector<TxIn> vin;
			std::vector<TxOut> vout;
			uint32_t nLockTime;

			ADD_SERIALABLE_FUNC_DEF;
			sha256 GetHash();
		};
	}
}