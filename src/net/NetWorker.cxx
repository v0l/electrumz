#include "NetWorker.h"
#include "JsonRPCServer.h"

#include <mbedtls/debug.h>
#include <spdlog/spdlog.h>
using namespace electrumz::net;

NetWorker::NetWorker(const char* ip, unsigned short port) {
	if (uv_loop_init(&this->loop)) {
		throw new std::exception("UV init failed");
	}
	else {
		uv_loop_set_data(&this->loop, this);
	}

	if (uv_tcp_init(&this->loop, &this->server)) {
		throw new std::exception("UV TCP init failed");
	}

	if (uv_ip4_addr(ip, port, &this->addr)) {
		throw new std::exception("Failed to parse ip");
	}

#ifndef ELECTRUMZ_NO_SSL
	this->ssl = (mbedtls_ssl_context*)malloc(sizeof(mbedtls_ssl_context));
	this->ssl_config = (mbedtls_ssl_config*)malloc(sizeof(mbedtls_ssl_config));

	memset(this->ssl, 0, sizeof(mbedtls_ssl_context));
	memset(this->ssl_config, 0, sizeof(mbedtls_ssl_config));

	mbedtls_ssl_init(this->ssl);
	mbedtls_ssl_config_init(this->ssl_config);
	mbedtls_ssl_config_defaults(this->ssl_config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);
	mbedtls_ssl_setup(this->ssl, this->ssl_config);

#ifdef _DEBUG
	mbedtls_debug_set_threshold(4);
	mbedtls_ssl_conf_dbg(this->ssl_config, [](void *ctx, int level, const char *file, int line, const char *str) {
		const_cast<char*>(str)[strlen(str) - 1] = 0; // remove newline

		switch (level) {
		case 1:
			spdlog::error(str);
			break;
		case 3:
			spdlog::info(str);
			break;
		case 4:
			spdlog::trace(str);
		default:
			spdlog::debug(str);
		}
	}, NULL);
#endif
#endif
}

NetWorker::~NetWorker() {
			
}

void NetWorker::Init() {
	this->worker_thread = std::thread(&NetWorker::Work, this);
}

void NetWorker::Work() {
	if (uv_tcp_bind(&this->server, (const struct sockaddr*)&this->addr, 0)) {
		throw new std::exception("Failed to bind port");
	}
					
	if (uv_listen((uv_stream_t*)&this->server, 128, [](uv_stream_t* s, int status) {
			auto nw = (NetWorker*)uv_loop_get_data(s->loop);
			nw->OnConnect(s, status);
	})) {
		throw new std::exception("Failed to listen on port");
	}

	if (uv_run(&this->loop, UV_RUN_DEFAULT)) {
		throw new std::exception("UV run failed..");
	}
}

void NetWorker::OnConnect(uv_stream_t* server, int status) {
	if (status < 0) {
		spdlog::error("Connect error: {}", uv_strerror(status));
		return;
	}

	uv_tcp_t *client = (uv_tcp_t*)malloc(sizeof(uv_tcp_t));
	uv_tcp_init(server->loop, client);
	if (uv_accept(server, (uv_stream_t*)client) == 0) {
		spdlog::info("New connection!");
		new JsonRPCServer(client);
	}
}

void NetWorker::Join() {
	this->worker_thread.join();
}