#include <electrumz/NetWorker.h>
#include <electrumz/JsonRPCServer.h>

#ifndef ELECTRUMZ_NO_SSL
#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#endif

#include <spdlog/spdlog.h>

using namespace electrumz::net;
using namespace electrumz::util;
using namespace electrumz::blockchain;

NetWorker::NetWorker(TXODB *db, Config *cfg) {
	this->db = db;
	this->cfg = cfg;

	if (uv_loop_init(&this->loop)) {
		spdlog::error("UV init failed");
		return;
	}
	else {
		uv_loop_set_data(&this->loop, this);
	}

	if (uv_tcp_init(&this->loop, &this->server)) {
		spdlog::error("UV TCP init failed");
		return;
	}

	if (uv_ip4_addr(cfg->host.c_str(), cfg->port, &this->addr)) {
		spdlog::error("Failed to parse ip");
		return;
	}

#ifndef ELECTRUMZ_NO_SSL
	if(!this->cfg->ssl_cert.empty() && !this->cfg->ssl_key.empty()){
		this->ssl_config = (mbedtls_ssl_config*)malloc(sizeof(mbedtls_ssl_config));
		this->ssl_cert = (mbedtls_x509_crt*)malloc(sizeof(mbedtls_x509_crt));
		this->ssl_key = (mbedtls_pk_context*)malloc(sizeof(mbedtls_pk_context));
		this->ssl_ctr_drbg = (mbedtls_ctr_drbg_context*)malloc(sizeof(mbedtls_ctr_drbg_context));
		this->ssl_entropy = (mbedtls_entropy_context*)malloc(sizeof(mbedtls_entropy_context));

		memset(this->ssl_config, 0, sizeof(mbedtls_ssl_config));
		memset(this->ssl_cert, 0, sizeof(mbedtls_x509_crt));
		memset(this->ssl_key, 0, sizeof(mbedtls_pk_context));
		memset(this->ssl_ctr_drbg, 0, sizeof(mbedtls_ctr_drbg_context));
		memset(this->ssl_entropy, 0, sizeof(mbedtls_entropy_context));

		int err = 0;
		mbedtls_ssl_config_init(this->ssl_config);
		mbedtls_x509_crt_init(this->ssl_cert);
		mbedtls_pk_init(this->ssl_key);
		mbedtls_entropy_init(this->ssl_entropy);
		mbedtls_ctr_drbg_init(this->ssl_ctr_drbg);

		mbedtls_ssl_config_defaults(this->ssl_config, MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT);

		//try to load certs
		if ((err = mbedtls_x509_crt_parse_file(this->ssl_cert, this->cfg->ssl_cert.c_str())) != 0) {
			char err_msg[255];
			mbedtls_strerror(err, err_msg, 255);

			spdlog::error("Failed to load cert: {}", err_msg);
			return;
		}

		if ((err = mbedtls_pk_parse_keyfile(this->ssl_key, this->cfg->ssl_key.c_str(), NULL)) != 0) {
			char err_msg[255];
			mbedtls_strerror(err, err_msg, 255);

			spdlog::error("Failed to load key: {}", err_msg);
			return;
		}

		mbedtls_ctr_drbg_seed(this->ssl_ctr_drbg, mbedtls_entropy_func, this->ssl_entropy, NULL, 0);
		mbedtls_ssl_conf_rng(this->ssl_config, mbedtls_ctr_drbg_random, this->ssl_ctr_drbg);
		mbedtls_ssl_conf_ca_chain(this->ssl_config, this->ssl_cert->next, NULL);
		mbedtls_ssl_conf_own_cert(this->ssl_config, this->ssl_cert, this->ssl_key);

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
		this->ssl_enabled = true;
	}
	else {
		spdlog::warn("SSL support is enabled, but no cert/key was specified..");
		this->ssl_config = nullptr;
	}
#endif
}

NetWorker::~NetWorker() {
			
}

void NetWorker::Init() {
	this->worker_thread = std::thread(&NetWorker::Work, this);
}

void NetWorker::Work() {
	int err = 0;
	if (err = uv_tcp_bind(&this->server, (const struct sockaddr*)&this->addr, 0)) {
		spdlog::error("Net worker failed to bind port: {}", uv_strerror(err));
		return;
	}
					
	if (err = uv_listen((uv_stream_t*)&this->server, 128, [](uv_stream_t* s, int status) {
			auto nw = (NetWorker*)uv_loop_get_data(s->loop);
			nw->OnConnect(s, status);
	})) {
		spdlog::error("Net worker failed to listen: {}", uv_strerror(err));
		return;
	}

	if (err = uv_run(&this->loop, UV_RUN_DEFAULT)) {
		spdlog::error("Failed to start net worker thread: {}", uv_strerror(err));
		return;
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
#ifndef ELECTRUMZ_NO_SSL
		new JsonRPCServer(client, this->cfg, this->ssl_config);
#else
		new JsonRPCServer(client);
#endif
	}
}

void NetWorker::Join() {
	this->worker_thread.join();
}

RPCClient NetWorker::CreateRPCClient(std::string addr, std::string uname, std::string pw) {
	RPCClient ret(addr, uname, pw);
	ret.Connect(&this->loop);
	return ret;
}
