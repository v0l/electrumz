#pragma once

#include <nlohmann/json.hpp>

namespace electrumz {
	namespace util {
		class Config {
		public: 
			Config(std::string);

			std::string host = "0.0.0.0";
			unsigned short port = 5555;

			//rpc details
			std::string rpchost;
			std::string rpcusername;
			std::string rpcpassword;

			//zmq details
			std::string zmqrawtx;
			std::string zmqrawblock;

#ifndef ELECTRUMZ_NO_SSL
			std::string ssl_cert;
			std::string ssl_key;
#endif
		private:
			nlohmann::json to_json();
			void from_json(std::ifstream &fi);
		};
	}
}