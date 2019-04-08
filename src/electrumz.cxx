#include <vector>
#include <algorithm>

#include <spdlog\spdlog.h>
#include "net\NetWorker.h"

using namespace electrumz;

int main(int argc, char* argv[]) {

	spdlog::info("Starting electrumz..");

	//std::thread::hardware_concurrency()
	std::vector<net::NetWorker*> v(1);
	std::transform(v.begin(), v.end(), v.begin(), [](net::NetWorker *w) {
		auto nw = new net::NetWorker("0.0.0.0");
		nw->Init();
		return nw;
	});

	//wait for exit
	for (auto t : v) {
		t->Join();
	}
	return 0;
}