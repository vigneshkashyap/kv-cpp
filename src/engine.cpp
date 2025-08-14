#include "engine.h"
#include <algorithm>
#include <iostream>
#include <cstdio>

namespace fs = std::filesystem;

Engine::Engine(std::string data_dir, size_t mem_flush_threshold_bytes)
    : data_dir_(std::move(data_dir))
    , flush_threshold_(mem_flush_threshold_bytes)
    , mem_()
    , wal_( (fs::path(data_dir_) / "wal.log").string() )
{}

Engine::~Engine() {}

std::optional<uint64_t> Engine::parse_id(const fs::path& p) {
    if (p.extension() != ".sst") return std::nullopt;
    const auto stem = p.stem().string();
    if (stem.size() == 0) return std::nullopt;
    try {
        return std::stoull(stem);
    } catch (...) {
        return std::nullopt;
    }
}

uint64_t Engine::next_file_id() const {
    uint64_t max_id = 0;
    for (const auto& t : tables_) {
        if (t) max_id = std::max(max_id, t->file_id());
    }
    // If no tables, scan directory once (first open)
    for (auto& de : fs::directory_iterator(data_dir_)) {
        if (!de.is_regular_file()) continue;
        auto id = parse_id(de.path());
        if (id) max_id = std::max(max_id, *id);
    }
    return max_id + 1;
}

bool Engine::load_existing_sstables() {
    tables_.clear();
    std::vector<std::pair<uint64_t, std::string>> files;
    for (auto& de : fs::directory_iterator(data_dir_)) {
        if (!de.is_regular_file()) continue;
        auto id = parse_id(de.path());
        if (!id) continue;
        files.emplace_back(*id, de.path().string());
    }
    // newest first
    std::sort(files.begin(), files.end(),
              [](const auto& a, const auto& b){ return a.first > b.first; });

    for (auto& [id, path] : files) {
        auto t = std::make_shared<SSTable>();
        if (t->Open(path)) {
            tables_.push_back(std::move(t));
        } else {
            std::cerr << "Warning: failed to open SSTable " << path << "\n";
        }
    }
    return true;
}

bool Engine::open() {
    std::error_code ec;
    fs::create_directories(data_dir_, ec);

    // 1) Load SSTables (newest -> oldest)
    if (!load_existing_sstables()) return false;

    // 2) Open WAL for appends
    if (!wal_.open()) return false;

    // 3) Replay WAL into MemTable
    {
        WAL reader( (fs::path(data_dir_) / "wal.log").string() );
        if (!reader.open()) return false;    // open read-only is fine (same format)
        if (!reader.replay(mem_)) return false;
    }
    return true;
}

bool Engine::flush() {
    // Snapshot memtable
    std::vector<std::pair<std::string, MemValue>> snap;
    mem_.snapshot(snap);

    if (snap.empty()) return true;

    // Ensure strict key ordering (works even if source is unordered_map)
    std::sort(snap.begin(), snap.end(),
              [](const auto& a, const auto& b){ return a.first < b.first; });

    // Dedup by key (should already be unique in map; harmless otherwise)
    snap.erase(std::unique(snap.begin(), snap.end(),
                [](const auto& a, const auto& b){ return a.first == b.first; }),
               snap.end());

    uint64_t id = next_file_id();
    std::string out_path;
    if (!SSTable::Build(data_dir_, id, snap, &out_path)) return false;

    // Open the new table and add to front (newest first)
    auto t = std::make_shared<SSTable>();
    if (!t->Open(out_path)) return false;
    tables_.insert(tables_.begin(), std::move(t));

    // Reset WAL and clear MemTable
    if (!wal_.reset()) return false;
    mem_.clear();
    return true;
}

bool Engine::flush_if_needed() {
    if (mem_.bytes() >= flush_threshold_) {
        return flush();
    }
    return true;
}

bool Engine::sync() {
    return wal_.sync();
}

bool Engine::put(const std::string& key, const std::string& value) {
    if (!wal_.appendPut(key, value)) return false;
    if (!mem_.put(key, value)) return false;
    return flush_if_needed();
}

bool Engine::del(const std::string& key) {
    if (!wal_.appendDel(key)) return false;
    if (!mem_.del(key)) return false;
    return flush_if_needed();
}

std::optional<std::string> Engine::get(const std::string& key) const {
    // 1) MemTable first
    if (auto mv = mem_.get(key)) {
        if (mv->type == RecType::Put) return mv->value;
        return std::nullopt; // Del tombstone
    }

    // 2) SSTables, newest -> oldest
    for (const auto& t : tables_) {
        std::string out;
        auto kind = t->Probe(key, &out);
        if (kind == SSTable::ProbeKind::Put) return out;
        if (kind == SSTable::ProbeKind::Tombstone) return std::nullopt; // stop search
        // else Absent: continue
    }
    return std::nullopt;
}

void Engine::list_tables() const {
    std::cout << "SSTables (newest->oldest): " << tables_.size() << "\n";
    for (const auto& t : tables_) {
        std::cout << "  " << t->path() << " (index=" << t->index_size() << ")\n";
    }
}
