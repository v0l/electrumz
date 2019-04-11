#include <electrumz\JsonRPCServer.h>

#if defined(_DEBUG) && !defined(ELECTRUMZ_NO_SSL)
#include <mbedtls\debug.h>
#endif
#include <spdlog\spdlog.h>
#include <nlohmann\json.hpp>
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

#ifndef ELECTRUMZ_NO_SSL
JsonRPCServer::JsonRPCServer(uv_tcp_t *s, mbedtls_ssl_config* ssl_cfg) {
	this->ssl_config = ssl_cfg;
#else
JsonRPCServer::JsonRPCServer(uv_tcp_t *s) {
#endif
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

#ifndef ELECTRUMZ_NO_SSL
int JsonRPCServer::TryHandshake() {

	auto hsr = mbedtls_ssl_handshake(this->ssl);
	if (hsr == 0) {
		this->state ^= JsonRPCState::SSL_HANDSHAKE; //handshake is done, remove handshaking flag
		return 2; //handshake done
	}
	else if (hsr == MBEDTLS_ERR_SSL_WANT_READ || hsr == MBEDTLS_ERR_SSL_WANT_WRITE) {
		return 1; //wait for more data
	}
	else {
		mbedtls_ssl_session_reset(this->ssl);
		//free this if it wasnt all used
		this->ssl_buf_offset = 0;
		this->ssl_buf_len = 0;
		this->ssl_buf = NULL;
		return 0; //something bad happen
	}
}
#endif

int JsonRPCServer::AppendBuffer(ssize_t nread, unsigned char* buf) {
	//make sure we have enough space to copy
	if (nread + this->offset > this->len) {
		auto min_size = this->offset + nread + 1;
		if (min_size > JSONRPC_MAX_BUFFER) {
			//close and exit
			return 0;
		}

		this->len = min_size + (min_size % JSONRPC_BUFF_LEN);
		if (this->buf == nullptr) {
			this->buf = (unsigned char*)malloc(this->len);
		}
		else {
			this->buf = (unsigned char*)realloc(this->buf, this->len);
		}
	}

	memcpy(this->buf + this->offset, buf, nread);
	this->offset += nread;
	return 1;
}

int JsonRPCServer::Write(ssize_t len, unsigned char* buf) {
#ifndef ELECTRUMZ_NO_SSL
	if (this->state & JsonRPCState::SSL_NORMAL) {
		return mbedtls_ssl_write(this->ssl, buf, len);
	}
#endif
	if (this->state & JsonRPCState::NORMAL) {
		return this->WriteInternal(len, buf);
	}

	return 0;
}

int JsonRPCServer::WriteInternal(ssize_t len, unsigned char* buf) {
	uv_buf_t *sbuf = new uv_buf_t[2];
	sbuf[0].base = (char*)malloc(len);
	sbuf[0].len = len;
	sbuf[1].base = (char*)this; //this will do as our data pointer
	sbuf[1].len = 0;

	memcpy(sbuf[0].base, buf, len);

	uv_write_t *req = new uv_write_t;
	uv_req_set_data((uv_req_t*)req, sbuf);

	if (uv_write(req, (uv_stream_t*)this->stream, sbuf, 1, [](uv_write_t* req, int status) {
		auto srv = (uv_buf_t(*)[2])uv_req_get_data((uv_req_t*)req);
		((JsonRPCServer*)srv[1]->base)->HandleWrite(req, status);
	})) {
		this->HandleWrite(req, 0);
		spdlog::error("Something went wrong");
		return 0;
	}
	return len;
}

int JsonRPCServer::HandleRead(ssize_t nread, const uv_buf_t* buf) {
	spdlog::info("Got {} bytes from socket.", nread);
	if (nread <= 0) {
		spdlog::error("Connection error: {}", uv_strerror(nread));
		return 0; //socket closed
	}

#ifndef ELECTRUMZ_NO_SSL
	//if we are in TLS mode set the pointer to our buffer for mbedtls read callback later
	if (this->state & JsonRPCState::SSL_NORMAL) {
		assert(this->ssl_buf == nullptr);
		this->ssl_buf = buf->base;
		this->ssl_buf_len = nread;
		this->ssl_buf_offset = 0;
	}
#endif

	//check for tls clienthello
	if (this->state & JsonRPCState::START) {
		this->state ^= JsonRPCState::START;
		if (this->IsTLSClientHello(nread, buf->base)) {
#ifndef ELECTRUMZ_NO_SSL
			//we are not in SSL mode yet so set this here
			assert(this->ssl_buf == nullptr);
			this->ssl_buf = buf->base;
			this->ssl_buf_len = nread;
			this->ssl_buf_offset = 0;

			//setup ssl context
			this->InitTLSContext();

			//start handshake
			this->state |= JsonRPCState::SSL_HANDSHAKE;
			return this->TryHandshake();
#else
			return 0; //SSL is not supported
#endif
		}
		else {
			this->state |= JsonRPCState::NORMAL;
		}
	}
#ifndef ELECTRUMZ_NO_SSL
	else if (this->state & JsonRPCState::SSL_HANDSHAKE) {
		return this->TryHandshake();
	}
#endif

	int buf_len = 0;
	unsigned char* buf_check = NULL;
#ifndef ELECTRUMZ_NO_SSL
	assert(this->state & JsonRPCState::SSL_NORMAL);

	buf_len = this->ssl_buf_len + (JSONRPC_BUFF_LEN - (this->ssl_buf_len % JSONRPC_BUFF_LEN));
	buf_check = (unsigned char*)malloc(buf_len);
	buf_len = mbedtls_ssl_read(this->ssl, buf_check, buf_len);
	if (buf_len == 0 || (buf_len < 0 && (buf_len != MBEDTLS_ERR_SSL_WANT_READ && buf_len != MBEDTLS_ERR_SSL_WANT_WRITE))) {
		free(buf_check);
		return 0;//end something went wrong
	}
	else if (buf_len == MBEDTLS_ERR_SSL_WANT_READ) {
		//just append to internal buffer and wait for more
		int ret = this->AppendBuffer(buf_len, buf_check);
		free(buf_check);
		return ret;
	}
#else
	assert(this->state & JsonRPCState::NORMAL);
	buf_check = (unsigned char*)buf->base;
	buf_len = nread;
#endif

	//detect http request
	char* http = strstr((char*)buf_check, "GET");
	if ((unsigned char*)http == buf_check) {
#ifndef ELECTRUMZ_NO_SSL
		free(buf_check);
#endif
		static char* http_rsp = "HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: 23\r\n\r\n<h2>ElectrumZ 1.0!</h2>";
		this->Write(strlen(http_rsp), (unsigned char*)http_rsp);
		return 0; //http isnt supported
	}

	char* nl = strchr((char*)buf_check, JSONRPC_DELIM);
	if (nl != nullptr) {
		nlohmann::json req;
		req.parse(nlohmann::detail::input_adapter(nl, nl - (char*)buf_check));

		spdlog::info("Got command: {}", req.dump());
		this->HandleCommand(req);
	}
	else {
		int ret = this->AppendBuffer(buf_len, buf_check);
#ifndef ELECTRUMZ_NO_SSL
		free(buf_check);
#endif
		return ret;
	}

#ifndef ELECTRUMZ_NO_SSL
	free(buf_check);
#endif
	return 1;
}

int JsonRPCServer::HandleCommand(nlohmann::json& cmd) {
	auto method = cmd["method"].get<std::string>();
	if (this->CommandMap.find(method) != this->CommandMap.end()) {
		auto method_mapped = this->CommandMap.at(method);
		switch (method_mapped) {
			case ElectrumCommands::BCBlockHeader: {
				
				break;
			}
			case ElectrumCommands::BCBlockHeaders: {
				break;
			}
			case ElectrumCommands::BCEstimatefee: {
				break;
			}
			case ElectrumCommands::BCHeadersSubscribe: {
				break;
			}
			case ElectrumCommands::BCRelayfee: {
				break;
			}
			case ElectrumCommands::SHGetBalance: {
				break;
			}
			case ElectrumCommands::SHGetHistory: {
				break;
			}
			case ElectrumCommands::SHGetMempool: {
				break;
			}
			case ElectrumCommands::SHHistory: {
				break;
			}
			case ElectrumCommands::SHListUnspent: {
				break;
			}
			case ElectrumCommands::SHSubscribe: {
				break;
			}
			case ElectrumCommands::SHUTXOS: {
				break;
			}
			case ElectrumCommands::TXBroadcast: {
				break;
			}
			case ElectrumCommands::TXGet: {
				break;
			}
			case ElectrumCommands::TXGetMerkle: {
				break;
			}
			case ElectrumCommands::TXIdFromPos: {
				break;
			}
			case ElectrumCommands::MPChanges: {
				break;
			}
			case ElectrumCommands::MPGetFeeHistogram: {
				break;
			}
			case ElectrumCommands::SVAddPeer: {
				break;
			}
			case ElectrumCommands::SVBanner: {
				break;
			}
			case ElectrumCommands::SVDonationAddress: {
				break;
			}
			case ElectrumCommands::SVFeatures: {
				break;
			}
			case ElectrumCommands::SVPeersSubscribe: {
				break;
			}
			case ElectrumCommands::SVPing: {
				break;
			}
			case ElectrumCommands::SVVersion: {
				break;
			}
			default: {
				//not possible???
				break;
			}
		}
	}
	else {
		//need something to write responses
	}
	return 0;
}

void JsonRPCServer::End() {
	uv_read_stop((uv_stream_t*)this->stream);

#ifndef ELECTRUMZ_NO_SSL
	mbedtls_ssl_free(this->ssl);
	free(this->ssl);
	this->ssl = NULL;
	this->ssl_config = NULL;
	this->ssl_buf = NULL;
#endif
	free(this->buf);
	this->buf = NULL;

	uv_close((uv_handle_t*)this->stream, [](uv_handle_t* h) {
		auto svr = (JsonRPCServer*)uv_handle_get_data(h);

		spdlog::info("Connection closed..");
		free(svr);
		free(h);
	});
}

bool JsonRPCServer::IsTLSClientHello(ssize_t nread, char* buf) {
	if (nread > 4 &&
		(buf[0] == 0x16 && buf[1] == 0x03) && //SSL3 or TLS
		(buf[2] >= 0x00 && buf[2] <= 0x03) //SSL3 or (TLS1.0, TLS1.1 or TLS1.2)
		) {
		return true;
	}
	return false;
}
#ifndef ELECTRUMZ_NO_SSL
void JsonRPCServer::InitTLSContext() {
	this->state |= JsonRPCState::SSL_NORMAL;
	
	this->ssl = (mbedtls_ssl_context*)malloc(sizeof(mbedtls_ssl_context));
	memset(this->ssl, 0, sizeof(mbedtls_ssl_context));

	mbedtls_ssl_init(this->ssl);
	mbedtls_ssl_setup(this->ssl, this->ssl_config);

	//set bio cbs, ideally we could just pass our alredy read buffer
	//but this is not possible with mbedtls (that i know of)
	mbedtls_ssl_set_bio(this->ssl, this, [](void *ctx, const unsigned char *buf, size_t len) {
		auto srv = (JsonRPCServer*)ctx;
		return srv->WriteInternal(len, const_cast<unsigned char*>(buf));
	}, [](void *ctx, unsigned char *buf, size_t len) {
		auto srv = (JsonRPCServer*)ctx;
		if (srv->ssl_buf_len == 0) {
			return MBEDTLS_ERR_SSL_WANT_READ;
		}

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
#endif

int JsonRPCServer::HandleWrite(uv_write_t* req, int status) {
	auto srv = (uv_buf_t(*)[2])uv_req_get_data((uv_req_t*)req);
	
	srv[1]->base = nullptr;
	free(srv[0]->base);
	delete[] srv;
	free(req);
	return 1;
}