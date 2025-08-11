#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>

enum class RecType : uint8_t { Put = 1, Del = 2 };

struct MemValue {
    RecType type;
    std::string value; // empty when Del
};

class MemTable {
public:
    bool put(std::string key, std::string value);
    bool del(std::string key);
    std::optional<MemValue> get(const std::string& key) const;

    size_t approxBytes() const { return bytes_; }
    bool empty() const { return kv_.empty(); }
    void clear();

    using Iter = std::map<std::string, MemValue>::const_iterator;
    Iter begin() const { return kv_.begin(); }
    Iter end()   const { return kv_.end(); }

private:
    std::map<std::string, MemValue> kv_; // ordered for future flush
    size_t bytes_ = 0;                   // rough size tracker
};
