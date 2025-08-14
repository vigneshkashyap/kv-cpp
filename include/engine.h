#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <filesystem>

#include "memtable.h"
#include "wal.h"
#include "sstable.h"

class Engine {
public:
    explicit Engine(std::string data_dir, size_t mem_flush_threshold_bytes = 4 * 1024 * 1024);
    ~Engine();

    bool open();        // load SSTables, open WAL, replay WAL -> MemTable
    bool flush();       // MemTable -> SSTable (V0), WAL reset, clear MemTable
    bool sync();        // fsync WAL

    // Mutations
    bool put(const std::string& key, const std::string& value);
    bool del(const std::string& key);

    // Lookup
    std::optional<std::string> get(const std::string& key) const;

    // Debug / info
    void list_tables() const;
    size_t mem_bytes() const { return mem_.bytes(); }
    size_t mem_size()  const { return mem_.size();  }

private:
    bool load_existing_sstables();          // scan dir, open *.sst newest->oldest
    uint64_t next_file_id() const;          // 1 + max existing id
    static std::optional<uint64_t> parse_id(const std::filesystem::path& p);

    bool flush_if_needed();                 // internal helper

private:
    std::string data_dir_;
    size_t flush_threshold_;

    mutable MemTable mem_;
    WAL wal_;                               // append WAL at data_dir_/wal.log

    // newest -> oldest
    std::vector<std::shared_ptr<SSTable>> tables_;
};
