#include "sstable.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <algorithm>

using std::string;
using std::string_view;
using std::vector;
namespace fs = std::filesystem;

// ===== low-level IO =====
bool SSTable::write_all(int fd, const void* p, size_t n) {
    const char* c = static_cast<const char*>(p);
    size_t left = n;
    while (left) {
        ssize_t w = ::write(fd, c, left);
        if (w < 0) return false;
        c += w; left -= w;
    }
    return true;
}
bool SSTable::read_all(int fd, void* p, size_t n) {
    char* c = static_cast<char*>(p);
    size_t left = n;
    while (left) {
        ssize_t r = ::read(fd, c, left);
        if (r <= 0) return false;
        c += r; left -= r;
    }
    return true;
}
bool SSTable::write_u32(int fd, uint32_t v){ return write_all(fd, &v, sizeof(v)); }
bool SSTable::write_u64(int fd, uint64_t v){ return write_all(fd, &v, sizeof(v)); }
bool SSTable::write_u8 (int fd, uint8_t v ){ return write_all(fd, &v, sizeof(v)); }
bool SSTable::read_u32 (int fd, uint32_t& v){ return read_all(fd, &v, sizeof(v)); }
bool SSTable::read_u64 (int fd, uint64_t& v){ return read_all(fd, &v, sizeof(v)); }
bool SSTable::read_u8  (int fd, uint8_t& v ){ return read_all(fd, &v, sizeof(v)); }

bool SSTable::fsync_dir(const string& dir) {
    int dfd = ::open(dir.c_str(), O_DIRECTORY | O_RDONLY);
    if (dfd < 0) return false;
    bool ok = (::fsync(dfd) == 0);
    ::close(dfd);
    return ok;
}

string SSTable::file_name_for(const string& dir, uint64_t id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%06llu.sst", static_cast<unsigned long long>(id));
    return (fs::path(dir) / buf).string();
}
string SSTable::tmp_name_for(const string& dir, uint64_t id) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "tmp_%06llu.sst", static_cast<unsigned long long>(id));
    return (fs::path(dir) / buf).string();
}

// ===== Build =====
bool SSTable::Build(const string& dir, uint64_t file_id,
                    const vector<std::pair<string, MemValue>>& entries,
                    string* out_final_path)
{
    // basic preconditions
    if (!entries.empty()) {
        for (size_t i = 1; i < entries.size(); ++i) {
            if (!(entries[i-1].first < entries[i].first)) {
                // must be strictly increasing keys
                return false;
            }
        }
    }
    fs::create_directories(dir);

    string tmp = tmp_name_for(dir, file_id);
    string fin = file_name_for(dir, file_id);

    int fd = ::open(tmp.c_str(), O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) return false;

    // Header
    if (!write_u32(fd, kMagic) || !write_u32(fd, kVersion)) { ::close(fd); return false; }

    vector<SSTIndexRec> sparse;
    sparse.reserve(entries.size() / kIndexInterval + 4);

    // Data section
    uint64_t data_start = ::lseek(fd, 0, SEEK_CUR);
    (void)data_start;
    for (size_t i = 0; i < entries.size(); ++i) {
        const auto& k = entries[i].first;
        const auto& mv = entries[i].second;

        // index every Kth entry
        if (i % kIndexInterval == 0) {
            uint64_t off = static_cast<uint64_t>(::lseek(fd, 0, SEEK_CUR));
            sparse.push_back(SSTIndexRec{ k, off });
        }

        uint32_t klen = static_cast<uint32_t>(k.size());
        uint8_t  type = static_cast<uint8_t>(mv.type);
        uint32_t vlen = (mv.type == RecType::Put) ? static_cast<uint32_t>(mv.value.size()) : 0;

        if (!write_u32(fd, klen)) { ::close(fd); return false; }
        if (!write_u8 (fd, type)) { ::close(fd); return false; }
        if (!write_u32(fd, vlen)) { ::close(fd); return false; }
        if (!write_all(fd, k.data(), klen)) { ::close(fd); return false; }
        if (vlen && !write_all(fd, mv.value.data(), vlen)) { ::close(fd); return false; }
    }

    // Record where index starts
    uint64_t index_offset = static_cast<uint64_t>(::lseek(fd, 0, SEEK_CUR));

    // Write sparse index
    for (const auto& rec : sparse) {
        uint32_t klen = static_cast<uint32_t>(rec.key.size());
        if (!write_u32(fd, klen)) { ::close(fd); return false; }
        if (!write_all(fd, rec.key.data(), klen)) { ::close(fd); return false; }
        if (!write_u64(fd, rec.offset)) { ::close(fd); return false; }
    }

    // Footer
    uint32_t index_count = static_cast<uint32_t>(sparse.size());
    if (!write_u64(fd, index_offset)) { ::close(fd); return false; }
    if (!write_u32(fd, index_count))  { ::close(fd); return false; }
    if (!write_u32(fd, kMagic))       { ::close(fd); return false; }
    if (!write_u32(fd, kVersion))     { ::close(fd); return false; }

    if (::fsync(fd) != 0) { ::close(fd); return false; }
    ::close(fd);

    // durable rename
    if (!fsync_dir(dir)) return false;
    if (::rename(tmp.c_str(), fin.c_str()) != 0) return false;
    if (!fsync_dir(dir)) return false;

    if (out_final_path) *out_final_path = fin;
    return true;
}

// ===== Open =====
bool SSTable::Open(const string& path) {
    path_ = path;
    // extract file_id from name if it looks like NNNNNN.sst
    try {
        auto stem = fs::path(path).stem().string(); // "000001"
        file_id_ = std::stoull(stem);
    } catch (...) {
        file_id_ = 0;
    }

    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;

    uint64_t index_off = 0;
    uint32_t index_cnt = 0;
    bool ok = read_footer(fd, index_off, index_cnt);
    if (!ok) { ::close(fd); return false; }

    ok = load_index(fd, index_off, index_cnt);
    ::close(fd);
    return ok;
}

bool SSTable::read_footer(int fd, uint64_t& index_off, uint32_t& index_count) const {
    off_t end = ::lseek(fd, 0, SEEK_END);
    if (end < (off_t)(sizeof(uint64_t) + 3*sizeof(uint32_t))) return false;
    off_t footer_start = end - (off_t)(sizeof(uint64_t) + 3*sizeof(uint32_t));
    if (::lseek(fd, footer_start, SEEK_SET) < 0) return false;

    uint32_t magic = 0, ver = 0;
    if (!read_u64(fd, index_off)) return false;
    if (!read_u32(fd, index_count)) return false;
    if (!read_u32(fd, magic)) return false;
    if (!read_u32(fd, ver)) return false;
    if (magic != kMagic || ver != kVersion) return false;
    return true;
}

bool SSTable::load_index(int fd, uint64_t index_off, uint32_t index_count) {
    if (::lseek(fd, (off_t)index_off, SEEK_SET) < 0) return false;
    index_.clear();
    index_.reserve(index_count);
    for (uint32_t i = 0; i < index_count; ++i) {
        uint32_t klen = 0;
        if (!read_u32(fd, klen)) return false;
        std::string key(klen, '\0');
        if (!read_all(fd, key.data(), klen)) return false;
        uint64_t off = 0;
        if (!read_u64(fd, off)) return false;
        index_.push_back(SSTIndexRec{ std::move(key), off });
    }
    return true;
}

// Binary search helper: find greatest index key <= target; return its offset.
// If none, return offset of first data entry (just after header).
static uint64_t index_seek_offset(const vector<SSTIndexRec>& idx, string_view key) {
    if (idx.empty()) return sizeof(uint32_t) * 2; // header size (magic, version)
    size_t lo = 0, hi = idx.size();
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (idx[mid].key <= key) lo = mid + 1; else hi = mid;
    }
    if (hi == 0) return sizeof(uint32_t) * 2;
    return idx[hi-1].offset;
}

// Scan forward from offset to find key.
SSTable::ScanResult
SSTable::scan_for_key(int fd, uint64_t start_off, string_view key, string* out) const {
    if (::lseek(fd, (off_t)start_off, SEEK_SET) < 0) return ScanResult::Absent;

    while (true) {
        uint32_t klen = 0, vlen = 0; uint8_t type = 0;
        // Try to read key_len; if EOF, stop.
        uint8_t peekbuf[4];
        ssize_t g = ::read(fd, peekbuf, sizeof(uint32_t));
        if (g == 0) return ScanResult::Absent;
        if (g != (ssize_t)sizeof(uint32_t)) return ScanResult::Absent;
        // we consumed 4 bytes; put them into klen
        std::memcpy(&klen, peekbuf, sizeof(uint32_t));

        if (!read_u8(fd, type)) return ScanResult::Absent;
        if (!read_u32(fd, vlen)) return ScanResult::Absent;

        std::string k(klen, '\0');
        if (!read_all(fd, k.data(), klen)) return ScanResult::Absent;

        if (k > key) return ScanResult::Absent; // we've passed the target; not found here

        if ((RecType)type == RecType::Put) {
            std::string v(vlen, '\0');
            if (!read_all(fd, v.data(), vlen)) return ScanResult::Absent;
            if (k == key) {
                if (out) *out = std::move(v);
                return ScanResult::Put;
            }
        } else { // Del
            // value_len should be 0; skip if not
            if (vlen) {
                if (::lseek(fd, vlen, SEEK_CUR) < 0) return ScanResult::Absent;
            }
            if (k == key) return ScanResult::Del;
        }
        // else continue loop
    }
}

std::optional<std::string> SSTable::Get(string_view key) const {
    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0) return std::nullopt;

    uint64_t off = index_seek_offset(index_, key);
    std::string out;
    ScanResult r = scan_for_key(fd, off, key, &out);
    ::close(fd);

    if (r == ScanResult::Put) return out;
    return std::nullopt; // Del or Absent => not found
}

SSTable::ProbeKind SSTable::Probe(std::string_view key, std::string* out) const {
    int fd = ::open(path_.c_str(), O_RDONLY);
    if (fd < 0) return ProbeKind::Absent;
    uint64_t off = index_seek_offset(index_, key);
    std::string tmp;
    ScanResult r = scan_for_key(fd, off, key, &tmp);
    ::close(fd);
    if (r == ScanResult::Put) { if (out) *out = std::move(tmp); return ProbeKind::Put; }
    if (r == ScanResult::Del) return ProbeKind::Tombstone;
    return ProbeKind::Absent;
}
