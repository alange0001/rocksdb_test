
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

#include <rocksdb/db.h>
#include <rocksdb/slice.h>
#include <rocksdb/options.h>
#include <rocksdb/utilities/options_util.h>

#include "args.h"
#include "util.h"

////////////////////////////////////////////////////////////////////////////////////

class KeySpace {
	public:
	KeySpace(uint64_t size_);

	private:
	uint64_t size;
};

KeySpace::KeySpace(uint64_t size_) {
	size = size_;
}

////////////////////////////////////////////////////////////////////////////////////

class DB {
	public:
	DB(Args *args_);
	~DB();
	void Open();
	void setKeySpace(KeySpace* key_space_);
	void BulkLoad();

	private:
	Args* args;
	rocksdb::DB* db = nullptr;
	bool is_open = false;

	rocksdb::Options  options;
	rocksdb::DBOptions db_opt;
	std::vector<rocksdb::ColumnFamilyDescriptor> cf_descs;
	std::vector<rocksdb::ColumnFamilyHandle*> handles;

	rocksdb::WriteOptions write_options;
	rocksdb::ReadOptions read_options;

	KeySpace* key_space = nullptr;

	rocksdb::CompressionType getCompressionType();
	void loadOptionsFile();
	void setDefaultOptions();
};

DB::DB(Args *args_) {
	spdlog::debug("create class DB");
	args = args_;
}

DB::~DB(){
	if (is_open){
		for (auto* handle : handles) {
			delete handle;
		}
		delete db;
	}
}

void DB::Open(){
	rocksdb::Status  s;

	options.create_if_missing = true;
	options.IncreaseParallelism(args->db_threads);

	if (args->db_config_file.length() > 0) {
		loadOptionsFile();
		s = rocksdb::DB::Open(db_opt, args->db_path, cf_descs, &handles, &db);
	} else {
		setDefaultOptions();
		s = rocksdb::DB::Open(options, args->db_path, &db);
	}
	if (! s.ok() ) throw new std::string(s.getState());

	spdlog::debug("open database OK!");
	is_open = true;

	s = db->Put(write_options, "key1", "value1");
	if (! s.ok() ) throw new std::string(s.getState());
}

void DB::loadOptionsFile() {
	rocksdb::Status s;

	spdlog::debug("open database using config file");

	s = rocksdb::LoadOptionsFromFile(args->db_config_file, rocksdb::Env::Default(), &db_opt,
            &cf_descs);
	if (! s.ok() ) throw new std::string(s.getState());
}

void DB::setDefaultOptions() {
	spdlog::debug("open database using default parameters");
	//using options.OptimizeLevelStyleCompaction()

	options.compaction_style = rocksdb::kCompactionStyleLevel;
	options.num_levels = 7;
	options.max_bytes_for_level_multiplier = 10;
	options.write_buffer_size = (args->db_memtables_budget * 1024 * 1024) / 4;
	options.target_file_size_base = (args->db_memtables_budget * 1024 * 1024) / 8;
	options.max_bytes_for_level_base = (args->db_memtables_budget * 1024 * 1024);

	options.min_write_buffer_number_to_merge = 2;
	options.max_write_buffer_number = 6;
	options.max_background_compactions = 4;
	options.level0_file_num_compaction_trigger = 2;
	options.level0_slowdown_writes_trigger = 4;
	options.level0_stop_writes_trigger = 6;

	// compress levels >= 2
	options.compression_per_level.resize(options.num_levels);
	auto compression_type = getCompressionType();
	for (int i = 0; i < options.num_levels; ++i) {
		if (i < 2) {
			options.compression_per_level[i] = rocksdb::kNoCompression;
		} else {
			options.compression_per_level[i] = compression_type;
		}
	}
}

rocksdb::CompressionType DB::getCompressionType() {
	spdlog::debug("using kLZ4Compression in getCompressionType()");
	return rocksdb::kLZ4Compression;
}

void DB::setKeySpace(KeySpace* key_space_) {
	key_space = key_space_;
}

void DB::BulkLoad() {

}


////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char** argv) {
	spdlog::info("Initializing program {}", argv[0]);

	try {
		auto args = Args(argc, argv);
		auto db = DB(&args);
		db.Open();
	}
	catch (std::string *str_error) {
		spdlog::error(*str_error);
		return 1;
	}

	spdlog::info("exit {}", 0);
	return 0;
}
