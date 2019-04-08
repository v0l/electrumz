#pragma once

#include <uv.h>
#include <mbedtls/ssl.h>
#include <thread>

namespace electrumz {
	namespace net {
		class NetWorker {
		public:
			NetWorker(const char* ip, unsigned short port = 5555);
			~NetWorker();
			void Init();
			void Join();

		private:
			void Work();
			void OnConnect(uv_stream_t* s, int status);

			std::thread worker_thread;
			uv_loop_t loop;
			uv_tcp_t server;
			sockaddr_in addr;

			mbedtls_ssl_context *ssl;
			mbedtls_ssl_config *ssl_config;
		};
	}
}