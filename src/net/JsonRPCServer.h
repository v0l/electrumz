#pragma once

#include <functional>
#include <uv.h>
#include <mbedtls\ssl.h>

namespace electrumz {
	namespace net {
		enum JsonRPCState {
			UNKNOWN = 0,
			START = 1, //flag for first read from socket
			NORMAL = 2, //flag for non-ssl connection
			SSL_NORMAL = 4, //flag for ssl connection
			SSL_HANDSHAKE = 8 //flag for ssl handshaking in progress
		};

		class JsonRPCServer {
		public:
			JsonRPCServer(uv_tcp_t*);
		private:
			int HandleRead(ssize_t, const uv_buf_t*);
			int HandleWrite(uv_write_t*, int);
			void End();
			bool IsTLSClientHello(ssize_t, char*);
			void InitTLSContext();

			//TLS stuff
			mbedtls_ssl_context *ssl;
			mbedtls_ssl_config *ssl_config;

			//We set this on recv and read it from our bio cbs
			char* ssl_buf = nullptr; 
			ssize_t ssl_buf_offset = 0;
			ssize_t ssl_buf_len = 0;

			//the connection
			uv_tcp_t *stream;
			int state;

			char *buf = nullptr;
			ssize_t offset = 0;
			ssize_t len = 0;
		};
	}
}