#include <vector>
#include <algorithm>

#include <spdlog\spdlog.h>

#include <electrumz\NetWorker.h>

using namespace electrumz;

int main(int argc, char* argv[]) {

	spdlog::info("Starting electrumz..");

	auto cfg = new util::Config("config.json");

	//std::thread::hardware_concurrency()
	std::vector<net::NetWorker*> v(1);
	std::transform(v.begin(), v.end(), v.begin(), [cfg](net::NetWorker *w) {
		auto nw = new net::NetWorker(cfg);
		nw->Init();
		return nw;
	});

	//wait for exit
	for (auto t : v) {
		t->Join();
	}
	return 0;
}