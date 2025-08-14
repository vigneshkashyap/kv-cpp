#include "memtable.h"

static size_t approxSizeOf(const std::string& k, const MemValue& mv) {
    return k.size() + (mv.type == RecType::Put ? mv.value.size() : 0) + 2;
}

bool MemTable::put(std::string key, std::string value) {
    if (auto it = kv_.find(key); it != kv_.end())
        bytes_ -= approxSizeOf(it->first, it->second);

    MemValue mv{RecType::Put, std::move(value)};
    bytes_ += approxSizeOf(key, mv);
    kv_[std::move(key)] = std::move(mv);
    return true;
}

bool MemTable::del(std::string key) {
    if (auto it = kv_.find(key); it != kv_.end())
        bytes_ -= approxSizeOf(it->first, it->second);

    MemValue mv{RecType::Del, {}};
    bytes_ += approxSizeOf(key, mv);
    kv_[std::move(key)] = std::move(mv);
    return true;
}

std::optional<MemValue> MemTable::get(const std::string& key) const {
    auto it = kv_.find(key);
    if (it == kv_.end()) return std::nullopt;
    return it->second;
}

void MemTable::clear() {
    kv_.clear();
    bytes_ = 0;
}

void MemTable::snapshot(std::vector<std::pair<std::string, MemValue>>& out) const {
    out.clear();
    out.reserve(kv_.size());
    for (const auto& kv : kv_) {
        out.emplace_back(kv.first, kv.second);
    }
}