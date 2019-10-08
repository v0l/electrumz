#pragma once

#include <uv.h>
#include <string>
#include <future>
#include <map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <http_parser.h>

#define RPC_READ_BUFF_SIZE 1024 * 16

namespace electrumz {
	namespace net {

		class RPCQuery {
		public:
			int id;
			std::shared_ptr<std::promise<nlohmann::json>> result; //json string
		};

		enum class RPCClientState {
			Init = 0,
			Connecting = 1,
			Connected = 2,
			Disconnecting = 3,
			Disconnected = 4
		};

		class RPCClient {
		public:
			RPCClient(const std::string& addr, const unsigned int& port, const std::string& username, const std::string& password)
				: address(addr), port(port), username(username), password(password), _state(RPCClientState::Init), socket(nullptr) {

				this->cmdId.store(0);
				this->_http_parser_settings = (http_parser_settings*)malloc(sizeof(http_parser_settings));
				this->_http_parser = (http_parser*)malloc(sizeof(http_parser));

				http_parser_settings_init(this->_http_parser_settings);
				http_parser_init(this->_http_parser, HTTP_RESPONSE);

				this->_http_parser->data = this;
				
				this->_http_parser_settings->on_body = [](http_parser* parser, const char* at, size_t length) {
					spdlog::trace("[RPC] http_on_body {}", length);
					auto self = (RPCClient*)parser->data;
					auto json_body = nlohmann::json::parse(nlohmann::detail::input_adapter(at, length));
					self->HandleResponse(std::move(json_body));
					return 0;
				};
			}
			~RPCClient() {
				uv_close((uv_handle_t*)(this->socket), [](uv_handle_t* h) {
					spdlog::info("[RPC] Closed..");
					});
				free(this->_http_parser_settings);
				free(this->_http_parser);
			}

			void Connect(uv_loop_t*);

			template<typename... Ts>
			std::future<nlohmann::json> Query(const char*, const Ts& ...);
			template<typename T>
			std::future<nlohmann::json> Query(const char*, const T&);
			std::future<nlohmann::json> Query(const char*);
		private:

			RPCClientState state() { std::scoped_lock lk(_stateLock); return _state; }
			const std::string stateStr(RPCClientState s) const {
				switch (s) {
				case RPCClientState::Init:
					return "Init";
				case RPCClientState::Connecting:
					return "Connecting";
				case RPCClientState::Connected:
					return "Connected";
				case RPCClientState::Disconnecting:
					return "Disconnecting";
				case RPCClientState::Disconnected:
					return "Disconnected";
				}
			}

			void state(RPCClientState ns) { 
				std::scoped_lock lk(_stateLock); 
				_state = ns; 
				spdlog::trace("[RPC] State changed to {}", stateStr(ns));
			}
			int WriteInternal(nlohmann::json&&);
			int WriteInternal(const unsigned char*, const ssize_t&);
			int HandleRead(ssize_t, const uv_buf_t*);
			int HandleResponse(nlohmann::json&&);
			std::future<nlohmann::json> SendQuery(nlohmann::json&&);

			void AddQuery(RPCQuery&& q) {
				std::scoped_lock lk(_queryMapLock);
				_queryMap.emplace(q.id, std::move(q));
			}
			void CompleteQuery(int id, nlohmann::json data) {
				std::scoped_lock lk(_queryMapLock);
				auto itr = _queryMap.find(id);
				if (itr != _queryMap.end()) {
					itr->second.result->set_value(data);
					_queryMap.erase(itr);
				}
				else {
					spdlog::warn("[RPC] Command not found: {}", id);
				}
			}

			RPCClientState _state;
			std::mutex _stateLock;

			uv_tcp_t* socket;
			std::string address;
			unsigned short port;
			std::string username;
			std::string password;

			std::map<int, RPCQuery> _queryMap;
			std::mutex _queryMapLock;

			std::atomic_int cmdId;

			http_parser_settings* _http_parser_settings;
			http_parser* _http_parser;
		};
	}
}