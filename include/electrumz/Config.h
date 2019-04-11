#pragma once

#include <nlohmann/json.hpp>
#include <iostream>

namespace electrumz {
	namespace util {
		class Config {
		public: 
			Config(std::string);

			std::string host = "0.0.0.0";
			unsigned short port = 5555;

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