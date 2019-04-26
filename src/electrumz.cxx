#include <vector>
#include <algorithm>

#include <argtable2.h>
#include <spdlog/spdlog.h>
#include <electrumz/NetWorker.h>
#include <electrumz/TXODB.h>

using namespace electrumz;
using namespace electrumz::blockchain;

struct arg_lit *help, *version;
struct arg_str *preload;
struct arg_end *end;

int main(int argc, char* argv[]) {
	void *argtable[] = {
        help    = arg_lit0("h", "help", "Display this help and exit"),
        version = arg_lit0(nullptr, "version", "Display version info and exit"),
        preload = arg_str0(nullptr, "preload", "<datadir>", "Preload DB from Bitcoin Core blocks dir"),
        end     = arg_end(20),
    };

	int nerrors = arg_parse(argc, argv, argtable);
	if(nerrors > 0 || help->count > 0){
		printf("Usage: electrumz");
        arg_print_syntax(stdout, argtable, "\n");
        printf("Bitcoin Electrum Server.\n\n");
        arg_print_glossary(stdout, argtable, "  %-25s %s\n");
		return nerrors > 0 ? 1 : 0;
	}

	spdlog::info("Starting electrumz..");
	auto cfg = new util::Config("config.json");
	auto db = new TXODB("db");
	if(db->Open()){
		return 0;
	}

	spdlog::info("LMDB Version: {}", db->GetLMDBVersion());
	if(preload->count > 0){
		auto preloadDir = std::string(*preload->sval);
		spdlog::info("Preloading DB from: {}", preloadDir);
		db->PreLoadBlocks(preloadDir);
	}
	

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