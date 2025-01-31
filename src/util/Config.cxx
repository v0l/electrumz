#include <electrumz/Config.h>

#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <fstream>

using namespace electrumz::util;

Config::Config(std::string path) {
	std::ifstream fi(path);
	if (fi.is_open()) {
		this->from_json(fi);
	}
	else {
		spdlog::warn("No config file found at {}, writing a default config and using defaults", path);

		//write sample config
		std::ofstream sample_config(path);
		if (!sample_config.fail()) {
			sample_config << std::setw(4) << this->to_json();
		}
		else {
			spdlog::error("Could not write sample config");
		}
	}
}

nlohmann::json Config::to_json() {
	nlohmann::json json;
	json["host"] = this->host;
	json["port"] = this->port;
#ifndef ELECTRUMZ_NO_SSL
	json["ssl_cert"] = this->ssl_cert;
	json["ssl_key"] = this->ssl_key;
#endif
	return json;
}

void Config::from_json(std::ifstream &fi) {
	nlohmann::json j;
	fi >> j;

	if (j["host"].is_string()) {
		this->host = j["host"].get<std::string>();
	}
	if (j["port"].is_number()) {
		this->port = j["port"].get<unsigned short>();
	}
	if (j["rpc_host"].is_string()) {
		this->rpchost = j["rpc_host"].get<std::string>();
	}
	if (j["rpc_port"].is_number()) {
		this->rpc_port = j["rpc_port"].get<unsigned short>();
	}
	if (j["rpc_username"].is_string()) {
		this->rpcusername = j["rpc_username"].get<std::string>();
	}
	if (j["rpc_password"].is_string()) {
		this->rpcpassword = j["rpc_password"].get<std::string>();
	}
	if (j["zmqrawtx"].is_string()) {
		this->zmqrawtx = j["zmqrawtx"].get<std::string>();
	}
	if (j["zmqrawblock"].is_string()) {
		this->zmqrawblock = j["zmqrawblock"].get<std::string>();
	}
#ifndef ELECTRUMZ_NO_SSL
	if (j["ssl_cert"].is_string()) {
		this->ssl_cert = j["ssl_cert"].get<std::string>();
	}
	if (j["ssl_key"].is_string()) {
		this->ssl_key = j["ssl_key"].get<std::string>();
	}
#endif
}