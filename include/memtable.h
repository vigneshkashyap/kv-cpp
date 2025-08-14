#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

enum class RecType : uint8_t { Put = 1, Del = 2 };

struct MemValue {
    RecType type;
    std::string value; // empty when Del
};

class MemTable {
public:
    // mutations
    bool put(std::string key, std::string value);
    bool del(std::string key);

    // lookup
    std::optional<MemValue> get(const std::string& key) const;

    // admin
    void   clear();
    bool   empty() const { return kv_.empty(); }
    size_t bytes() const { return bytes_; }           // engine uses this
    size_t size()  const { return kv_.size(); }       // engine uses this

    // optional iteration (useful for debugging)
    using Iter = std::map<std::string, MemValue>::const_iterator;
    Iter begin() const { return kv_.begin(); }
    Iter end()   const { return kv_.end(); }

    // flush helper (engine calls this)
    void snapshot(std::vector<std::pair<std::string, MemValue>>& out) const;

    // (optional) keep this ONLY if other files still call approxBytes()
    // [[deprecated("Use bytes() instead")]]
    // size_t approxBytes() const { return bytes(); }

private:
    std::map<std::string, MemValue> kv_;  // ordered for flush → SSTable
    size_t bytes_ = 0;                     // rough size tracker
};
