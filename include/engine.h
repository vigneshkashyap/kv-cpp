#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

// Stage 2 engine interface:
// - In-memory map
// - WAL append for each write
// - Full snapshot + WAL truncation
class Engine {
public:
    explicit Engine(std::string data_dir);
    // basic ops
    void put(const std::string& key, const std::string& value);
    void del(const std::string& key);
    std::string get(const std::string& key) const;

    // maintenance
    void snapshot();

    // (exposed for CLI convenience)
    const std::unordered_map<std::string, std::string>& state() const { return mem_; }

private:
    enum class RecType : uint8_t { Put = 1, Del = 2 };

    std::string dir_;
    std::string wal_path_;
    std::string snapshot_path_;
    std::unordered_map<std::string, std::string> mem_;

    // helpers
    static void write_u32(std::ofstream& f, uint32_t x);
    static uint32_t read_u32(std::ifstream& f);

    void append_record(RecType ty, const std::string& k, const std::string& v);
    void recover();
};
