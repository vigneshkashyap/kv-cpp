#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Reuse your existing types
#include "memtable.h"  // expects: enum class RecType { Put=1, Del=2 }; struct MemValue { RecType type; std::string value; }

struct SSTIndexRec {
    std::string key;  // full key of the indexed entry
    uint64_t offset;  // absolute file offset to the start of the entry in data section
};

class SSTable {
   public:
    // Build a new table from a **sorted** and **deduplicated** snapshot.
    // For each key, include exactly one MemValue; key order must be strict lexicographic ascending.
    static bool Build(const std::string& dir, uint64_t file_id,
                      const std::vector<std::pair<std::string, MemValue>>& entries,
                      std::string* out_final_path = nullptr);

    // Open an existing table (e.g., "data/000001.sst"); loads sparse index into memory.
    bool Open(const std::string& path);

    // Lookup key in this table. Returns:
    //   - std::optional<std::string>{"value"} if found as Put
    //   - std::optional<std::string>{} (nullopt) if found as Del (tombstone) or absent
    std::optional<std::string> Get(std::string_view key) const;

    const std::string& path() const { return path_; }
    uint64_t file_id() const { return file_id_; }
    size_t index_size() const { return index_.size(); }

    enum class ProbeKind { Absent,
                           Tombstone,
                           Put };

    // Probe key with tombstone awareness. If Put, fills *out.
    ProbeKind Probe(std::string_view key, std::string* out) const;

   private:
    // --- On-disk layout (V0) ---
    // Header:
    //   u32 magic 'KVST' (0x4B565354), u32 version=1
    // Data section: for each entry (sorted by key)
    //   u32 key_len, u8 type, u32 value_len, key bytes, value bytes
    // Sparse index: every K entries (K=64 by default)
    //   repeated: u32 key_len, key bytes, u64 file_offset
    // Footer (fixed size):
    //   u64 index_offset, u32 index_count, u32 magic, u32 version

    static constexpr uint32_t kMagic = 0x4B565354;  // 'K''V''S''T'
    static constexpr uint32_t kVersion = 1;
    static constexpr uint32_t kIndexInterval = 64;  // every 64 entries add an index record

    // helpers
    static bool fsync_dir(const std::string& dir);
    static bool write_all(int fd, const void* p, size_t n);
    static bool read_all(int fd, void* p, size_t n);
    static bool write_u32(int fd, uint32_t v);
    static bool write_u64(int fd, uint64_t v);
    static bool write_u8(int fd, uint8_t v);
    static bool read_u32(int fd, uint32_t& v);
    static bool read_u64(int fd, uint64_t& v);
    static bool read_u8(int fd, uint8_t& v);

    static std::string file_name_for(const std::string& dir, uint64_t id);
    static std::string tmp_name_for(const std::string& dir, uint64_t id);

    // open-time helpers
    bool read_footer(int fd, uint64_t& index_off, uint32_t& index_count) const;
    bool load_index(int fd, uint64_t index_off, uint32_t index_count);

    // scan from offset for target key (returns Put/Del/Absent)
    enum class ScanResult { Absent,
                            Put,
                            Del };
    ScanResult scan_for_key(int fd, uint64_t start_off, std::string_view key, std::string* out) const;

   private:
    std::string path_;
    uint64_t file_id_ = 0;
    std::vector<SSTIndexRec> index_;
};
