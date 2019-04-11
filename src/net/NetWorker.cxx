#include <electrumz\NetWorker.h>
#include <electrumz\JsonRPCServer.h>

#ifndef ELECTRUMZ_NO_SSL
#include <mbedtls/debug.h>
#include <mbedtls/error.h>
#endif

#include <spdlog/spdlog.h>
using namespace electrumz::net;
using namespace electrumz::util;

NetWorker::NetWorker(Config *cfg) {
	if (uv_loop_init(&this->loop)) {
		throw new std::exception("UV init failed");
	}
	else {
		uv_loop_set_data(&this->loop, this);
	}

	if (uv_tcp_init(&this->loop, &this->server)) {
		throw new std::exception("UV TCP init failed");
	}

	if (uv_ip4_addr(cfg->host.c_str(), cfg->port, &this->addr)) {
		throw new std::exception("Failed to parse ip");
	}

#ifndef ELECTRUMZ_NO_SSL
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
	if ((err = mbedtls_x509_crt_parse_file(this->ssl_cert, "C:\\Users\\kieran\\Documents\\GitHub\\electrumz\\build\\Debug\\localhost.crt")) != 0) {
		char err_msg[255];
		mbedtls_strerror(err, err_msg, 255);

		spdlog::error("Failed to load cert: {}", err_msg);
		throw new std::exception("Failed to load SSL cert");
	}

	if ((err = mbedtls_pk_parse_keyfile(this->ssl_key, "C:\\Users\\kieran\\Documents\\GitHub\\electrumz\\build\\Debug\\localhost.key", NULL)) != 0) {
		char err_msg[255];
		mbedtls_strerror(err, err_msg, 255);

		spdlog::error("Failed to load key: {}", err_msg);
		throw new std::exception("Failed to load SSL key");
	}

	mbedtls_ctr_drbg_seed(this->ssl_ctr_drbg, mbedtls_entropy_func, this->ssl_entropy, NULL, 0);
	mbedtls_ssl_conf_rng(this->ssl_config, mbedtls_ctr_drbg_random, this->ssl_ctr_drbg);
	mbedtls_ssl_conf_ca_chain(this->ssl_config, this->ssl_cert->next, NULL);
	mbedtls_ssl_conf_own_cert(this->ssl_config, this->ssl_cert, this->ssl_key);

#ifdef _DEBUG
	mbedtls_debug_set_threshold(1);
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
	int err = 0;
	if (err = uv_tcp_bind(&this->server, (const struct sockaddr*)&this->addr, 0)) {
		spdlog::error("Net worker failed to bind port: {}", uv_strerror(err));
		throw new std::exception("Failed to bind port");
	}
					
	if (err = uv_listen((uv_stream_t*)&this->server, 128, [](uv_stream_t* s, int status) {
			auto nw = (NetWorker*)uv_loop_get_data(s->loop);
			nw->OnConnect(s, status);
	})) {
		spdlog::error("Net worker failed to listen: {}", uv_strerror(err));
		throw new std::exception("Failed to listen on port");
	}

	if (err = uv_run(&this->loop, UV_RUN_DEFAULT)) {
		spdlog::error("Failed to start net worker thread: {}", uv_strerror(err));
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
#ifndef ELECTRUMZ_NO_SSL
		new JsonRPCServer(client, this->ssl_config);
#else
		new JsonRPCServer(client);
#endif
	}
}

void NetWorker::Join() {
	this->worker_thread.join();
}