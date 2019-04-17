#include <vector>
#include <algorithm>

#include <spdlog\spdlog.h>
#include <electrumz\NetWorker.h>
#include <electrumz\TXODB.h>

using namespace electrumz;
using namespace electrumz::blockchain;

int main(int argc, char* argv[]) {

	spdlog::info("Starting electrumz..");

	auto cfg = new util::Config("config.json");
	auto db = new TXODB("db");

	spdlog::info("LMDB Version: {}", db->GetLMDBVersion());
	db->PreLoadBlocks("D:\\Bitcoin\\blocks");

	//std::thread::hardware_concurrency()
	std::vector<net::NetWorker*> v(1);
	std::transform(v.begin(), v.end(), v.begin(), [db, cfg](net::NetWorker *w) {
		auto nw = new net::NetWorker(db, cfg);
		nw->Init();
		return nw;
	});

	//wait for exit
	for (auto t : v) {
		t->Join();
	}
	return 0;
}