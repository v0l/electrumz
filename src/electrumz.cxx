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

#ifdef _DEBUG
	spdlog::set_level(spdlog::level::level_enum::debug);
#endif
	spdlog::info("Starting electrumz..");
	auto cfg = new util::Config("config.json");
	auto db = new TXODB("db");
	if(db->Open()){
		return 0;
	}
	else {
		MDB_stat stat;
		if (db->GetTXOStats(&stat, DBI_TXO)) {
			return 0;
		}
		spdlog::info("TXO Keys: {0:n}", stat.ms_entries);

		if (db->GetTXOStats(&stat, DBI_ADDR)) {
			return 0;
		}
		spdlog::info("ADDR Keys: {0:n}", stat.ms_entries);

		if (db->GetTXOStats(&stat, DBI_BLK)) {
			return 0;
		}
		spdlog::info("BLK Keys: {0:n}", stat.ms_entries);
	}

	spdlog::info("LMDB Version: {}", db->GetLMDBVersion());
	if(preload->count > 0){
		auto preloadDir = std::string(*preload->sval);
		spdlog::info("Preloading DB from: {}", preloadDir);
		db->PreLoadBlocks(preloadDir);
		return 0;
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