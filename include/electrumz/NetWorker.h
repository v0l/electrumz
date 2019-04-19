#pragma once

#include <electrumz/Config.h>
#include <electrumz/TXODB.h>

#include <uv.h>
#ifndef ELECTRUMZ_NO_SSL
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#endif
#include <thread>

using namespace electrumz::util;
using namespace electrumz::blockchain;

namespace electrumz {
	namespace net {
		class NetWorker {
		public:
			NetWorker(TXODB*, Config*);
			~NetWorker();
			void Init();
			void Join();

		private:
			void Work();
			void OnConnect(uv_stream_t *s, int status);

			Config* cfg;
			TXODB *db;

			bool ssl_enabled;
			std::thread worker_thread;
			uv_loop_t loop;
			uv_tcp_t server;
			sockaddr_in addr;

#ifndef ELECTRUMZ_NO_SSL
			mbedtls_entropy_context *ssl_entropy;
			mbedtls_ctr_drbg_context *ssl_ctr_drbg;
			mbedtls_ssl_config *ssl_config;
			mbedtls_x509_crt *ssl_cert;
			mbedtls_pk_context *ssl_key;
#endif
		};
	}
}