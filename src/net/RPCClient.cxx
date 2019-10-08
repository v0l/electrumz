#include <electrumz/RPCClient.h>

#include <uv.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <electrumz/bitcoin/util_strencodings.h>

using namespace electrumz::net;

void RPCClient::Connect(uv_loop_t* loop) {
	auto state = this->state();

	if (state != RPCClientState::Init && state != RPCClientState::Disconnected) {
		spdlog::warn("[RPC] is already in state: {}", stateStr(state));
		return;
	}
	this->state(RPCClientState::Connecting);

	int err = 0;
	uv_tcp_t* socket = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	if (err = uv_tcp_init(loop, socket)) {
		spdlog::error("[RPC] tcp_init failed: {}", uv_strerror(err));
		this->state(RPCClientState::Disconnected);
		return;
	}

	uv_connect_t* connect = (uv_connect_t*)malloc(sizeof(uv_connect_t));
	uv_handle_set_data((uv_handle_t*)connect, this);

	struct sockaddr_in dest;
	if (err = uv_ip4_addr(this->address.c_str(), this->port, &dest)) {
		spdlog::error("[RPC] Failed to parse addr: {}, {}", this->address, uv_strerror(err));
		this->state(RPCClientState::Disconnected);
		return;
	}

	if (err = uv_tcp_connect(connect, socket, (const struct sockaddr*) & dest, [](uv_connect_t* req, int status) {
		auto self = (RPCClient*)uv_handle_get_data((uv_handle_t*)req);
		auto err2 = 0;

		if (status != 0) {
			spdlog::error("[RPC] Connect failed: {}", uv_strerror(status));
			self->state(RPCClientState::Disconnected);
			return;
		}

		self->socket = (uv_tcp_t*)req->handle;
		uv_handle_set_data((uv_handle_t*)self->socket, self); //pass this ref
		
		self->state(RPCClientState::Connected);
		spdlog::info("[RPC] connected!");

		if (err2 = uv_read_start((uv_stream_t*)self->socket, [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
			buf->base = (char*)malloc(RPC_READ_BUFF_SIZE);
			buf->len = RPC_READ_BUFF_SIZE;
			}, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
				if (nread <= 0) {
					spdlog::error("[RPC] Connection error: {}", uv_strerror(nread));
					return;
				}
				auto rpc = (RPCClient*)uv_handle_get_data((uv_handle_t*)stream);
				if (!rpc->HandleRead(nread, buf)) {
					// do disconnect
				}
			})) {
			spdlog::error("[RPC] Read start failed: {}", uv_strerror(err2));
			self->state(RPCClientState::Disconnected);
			return;
		}

		self->Query("getblockchaininfo");
		})) {
		spdlog::error("[RPC] Connect failed: {}", uv_strerror(err));
		this->state(RPCClientState::Disconnected);
		return;
	}
}

int RPCClient::HandleRead(ssize_t nread, const uv_buf_t* buf) {
	spdlog::trace("[RPC] Got {0:n} bytes", nread);
	spdlog::trace("[RPC] {}", std::string(buf->base, nread));

	auto nparsed = http_parser_execute(this->_http_parser, this->_http_parser_settings, buf->base, nread);
	spdlog::trace("[RPC] http_parsed {}", nparsed);
	return 1;
}

int RPCClient::HandleResponse(nlohmann::json&& j) {
	if (!j.at("error").is_null()) {
		return 0;
	}

	auto id = j["id"].get<int>();
	this->CompleteQuery(id, j["result"]);

	return 1;
}

std::future<nlohmann::json> RPCClient::SendQuery(nlohmann::json&& j) {
	auto id = this->cmdId++;

	auto ft = std::make_shared<std::promise<nlohmann::json>>();
	RPCQuery future{
		id,
		ft
	};
	this->AddQuery(std::move(future));

	j["id"] = id;
	j["jsonrpc"] = "2.0";

	this->WriteInternal(std::move(j));
	return ft->get_future();
}

template<typename... Ts>
std::future<nlohmann::json> RPCClient::Query(const char* method, const Ts& ...args) {
	auto cmd = nlohmann::json{
		{ "method", method },
		{ "params", { args... } }
	};

	return this->SendQuery(std::move(cmd));
}

template<typename T>
std::future<nlohmann::json> RPCClient::Query(const char* method, const T& args) {
	auto cmd = nlohmann::json{
		{ "method", method },
		{ "params", args }
	};

	return this->SendQuery(std::move(cmd));
}

std::future<nlohmann::json> RPCClient::Query(const char* method) {
	auto cmd = nlohmann::json{
		{ "method", method }
	};

	return this->SendQuery(std::move(cmd));
}

int RPCClient::WriteInternal(nlohmann::json&& j) {
	auto d = j.dump(-1, ' ', true);

	return this->WriteInternal((unsigned char*)d.data(), d.size());
}

int RPCClient::WriteInternal(const unsigned char* data, const ssize_t& len) {
	auto err = 0;

	//add req header
	auto header = fmt::format("POST / HTTP/1.1\r\nContent-Type: application/json\r\nContent-Length: {}\r\nConnection: keep-alive\r\nAuthorization: Basic {}\r\n\r\n", len, EncodeBase64(fmt::format("{}:{}", this->username, this->password)));
	
	auto bufLen = len + header.size();
	auto nbuf = (char*)malloc(bufLen);
	if (nbuf == 0) {
		spdlog::critical("Out of memory");
		return 0;
	}
	memcpy(nbuf, header.data(), header.size());
	memcpy(nbuf + header.size(), data, len);

	uv_buf_t buf[] = {
		{ bufLen, nbuf },
		{ 0, (char*)this } //not used
	};

	spdlog::trace("[RPC] Sending HTTP req: {}", std::string(nbuf, bufLen));

	auto req = new uv_write_t();
	uv_req_set_data((uv_req_t*)req, nbuf);

	if (err = uv_write(req, (uv_stream_t*)this->socket, buf, 1, [](uv_write_t* req, int status) {
		auto srv = (char*)uv_req_get_data((uv_req_t*)req);
		free(srv); //free the buffer we created for sending the message
		})) {
		spdlog::error("[RPC] Failed to write: {}", uv_strerror(err));
	}

	return err;
}