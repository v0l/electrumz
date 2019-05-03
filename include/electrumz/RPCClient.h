#pragma once

#include <uv.h>
#include <string>

namespace electrumz {
	namespace net {
		class RPCClient {
		public:
			RPCClient(const std::string addr, const std::string username, const std::string password)
				: address(addr), username(username), password(password) {

			}

			void Connect(uv_loop_t*);
		private:
			uv_loop_t* loop;
			std::string address;
			std::string username;
			std::string password;
		};
	}
}