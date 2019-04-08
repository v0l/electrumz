#include "JsonRPCServer.h"

#ifdef _DEBUG
#include <mbedtls\debug.h>
#endif
#include <spdlog\spdlog.h>
#include <assert.h>

using namespace electrumz::net;

//buffer size for reading client requests
#ifndef JSONRPC_BUFF_LEN
#define JSONRPC_BUFF_LEN 1024
#endif

//max size of data to be buffered before closing the connection
#ifndef JSONRPC_MAX_BUFFER
#define JSONRPC_MAX_BUFFER 16384
#endif

//delim for each json rcp command
#ifndef JSONRPC_DELIM
#define JSONRPC_DELIM '\n'
#endif

JsonRPCServer::JsonRPCServer(uv_tcp_t *s) {
	this->stream = s;
	this->state = JsonRPCState::START;
	
	//ref this class to track req/reply
	uv_handle_set_data((uv_handle_t*)s, this);

	uv_read_start((uv_stream_t*)s, [](uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
		buf->base = (char*)malloc(JSONRPC_BUFF_LEN);
		buf->len = JSONRPC_BUFF_LEN;
	}, [](uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf) {
		auto rpc = (JsonRPCServer*)uv_handle_get_data((uv_handle_t*)stream);
		if (!rpc->HandleRead(nread, buf)) {
			rpc->End();
		}
	});
}

int JsonRPCServer::HandleRead(ssize_t nread, const uv_buf_t* buf) {
	//if we are in TLS mode set the pointer to our buffer for mbedtls read callback later
	if (this->state & JsonRPCState::SSL_NORMAL) {
		assert(this->ssl_buf == nullptr);
		this->ssl_buf = buf->base;
		this->ssl_buf_len = nread;
		this->ssl_buf_offset = 0;
	}

	//check for tls clienthello
	if (this->state & JsonRPCState::START) {
		if (this->IsTLSClientHello(nread, buf->base)) {
			//we are not in SSL mode yet so set this here
			assert(this->ssl_buf == nullptr);
			this->ssl_buf = buf->base;
			this->ssl_buf_len = nread;
			this->ssl_buf_offset = 0;

			//setup ssl context
			this->InitTLSContext();

			//start handshake
			this->state |= JsonRPCState::SSL_HANDSHAKE;
			auto hsr = mbedtls_ssl_handshake(this->ssl);
			if (hsr == 0) {
				this->state ^= JsonRPCState::SSL_HANDSHAKE; //handshake is done, remove handshaking flag
			}
			else if (hsr == MBEDTLS_ERR_SSL_WANT_READ || hsr == MBEDTLS_ERR_SSL_WANT_WRITE) {
				return 1; //wait for more data
			}
			else {
				//free this if it wasnt all used
				this->ssl_buf_offset = 0;
				this->ssl_buf_len = 0;
				if (this->ssl_buf != nullptr) {
					free(this->ssl_buf);
				}
				return 0; //something bad happen
			}
		}
		else {
			this->state |= JsonRPCState::NORMAL;
		}

		this->state ^= JsonRPCState::START;
	}
	else if (this->state & JsonRPCState::SSL_HANDSHAKE) {

	}

	char* buf_check = NULL;
	//check if we have some buffered data already
	if (this->offset > 0) {
		//make sure we have enough space to copy
		if (nread + this->offset > this->len) {
			auto min_size = this->offset + nread + 1;
			if (min_size > JSONRPC_MAX_BUFFER) {
				//close and exit
				free(buf->base);
				return 0;
			}
			this->buf = (char*)realloc(this->buf, min_size + (min_size % JSONRPC_BUFF_LEN));
		}
	}

	char* nl = strchr(buf->base, JSONRPC_DELIM);
	if (nl != nullptr) {

	}

	return 1;
}

void JsonRPCServer::End() {
	//uv_read_stop(this->stream);
}

bool JsonRPCServer::IsTLSClientHello(ssize_t nread, char* buf) {
	if (nread > 4 &&
		(buf[0] == MBEDTLS_SSL_MSG_HANDSHAKE && buf[1] == MBEDTLS_SSL_MAJOR_VERSION_3) && //SSL3 or TLS
		(buf[2] >= MBEDTLS_SSL_MINOR_VERSION_0 && buf[2] <= MBEDTLS_SSL_MINOR_VERSION_3) //SSL3 or (TLS1.0, TLS1.1 or TLS1.2)
		) {
		return true;
	}
	return false;
}

void JsonRPCServer::InitTLSContext() {
	this->state |= JsonRPCState::SSL_NORMAL;
	

	//set bio cbs, ideally we could just pass our alredy read buffer
	//but this is not possible with mbedtls (that i know of)
	mbedtls_ssl_set_bio(this->ssl, this, [](void *ctx, const unsigned char *buf, size_t len) {
		auto srv = (JsonRPCServer*)ctx;
		
		uv_buf_t sbuf[1];
		sbuf[0].base = (char*)malloc(len);
		sbuf[0].len = len;

		memcpy(sbuf[0].base, buf, len);

		uv_write_t *req = new uv_write_t;
		uv_handle_set_data((uv_handle_t*)req, ctx);

		if (uv_write(req, (uv_stream_t*)srv->stream, sbuf, 1, [](uv_write_t* req, int status) {
			auto srv = (JsonRPCServer*)uv_req_get_data((uv_req_t*)req);
			srv->HandleWrite(req, status);
		})) {
			free(req);
			delete[] sbuf;
			spdlog::error("Something went wrong");
			return 0;
		}
		return (int)len;
	}, [](void *ctx, unsigned char *buf, size_t len) {
		auto srv = (JsonRPCServer*)ctx;

		auto rlen = min(srv->ssl_buf_len - srv->ssl_buf_offset, len);
		memcpy(buf, srv->ssl_buf + srv->ssl_buf_offset, rlen);

		if (rlen == srv->ssl_buf_len - srv->ssl_buf_offset) {
			//free the libuv buffer we dont need it now
			free(srv->ssl_buf);
			srv->ssl_buf = nullptr;
			srv->ssl_buf_len = 0;
			srv->ssl_buf_offset = 0;
		}
		else {
			srv->ssl_buf_offset += rlen;
		}

		return (int)rlen;
	}, NULL);
}

int JsonRPCServer::HandleWrite(uv_write_t* req, int status) {
	return 1;
}