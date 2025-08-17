#include "wal.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <vector>

#include "utils.h"

namespace {
constexpr uint32_t kMagic = 0x4B56574C;  // 'K''V''W''L' (KV WAL)
constexpr uint32_t kVersion = 1;

static bool writeU32(int fd, uint32_t v) {
    return ::write(fd, &v, sizeof(v)) == (ssize_t)sizeof(v);
}
static bool writeU8(int fd, uint8_t v) {
    return ::write(fd, &v, sizeof(v)) == (ssize_t)sizeof(v);
}
static bool readU32(int fd, uint32_t& v) {
    return ::read(fd, &v, sizeof(v)) == (ssize_t)sizeof(v);
}
static bool readU8(int fd, uint8_t& v) {
    return ::read(fd, &v, sizeof(v)) == (ssize_t)sizeof(v);
}
}  // namespace

WAL::WAL(std::string path) : path_(std::move(path)) {}
WAL::~WAL() {
    if (fd_ >= 0) ::close(fd_);
}

bool WAL::open() {
    std::filesystem::path p(path_);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    fd_ = ::open(path_.c_str(), O_RDWR | O_CREAT, 0644);
    if (fd_ < 0) return false;
    return validateHeader();
}

bool WAL::validateHeader() {
    off_t end = ::lseek(fd_, 0, SEEK_END);
    if (end == 0) {
        if (::lseek(fd_, 0, SEEK_SET) < 0) return false;
        if (!writeU32(fd_, kMagic)) return false;
        if (!writeU32(fd_, kVersion)) return false;
        return true;
    }
    if (::lseek(fd_, 0, SEEK_SET) < 0) return false;
    uint32_t m = 0, v = 0;
    if (!readU32(fd_, m)) return false;
    if (!readU32(fd_, v)) return false;
    if (m != kMagic || v != kVersion) return false;
    if (::lseek(fd_, 0, SEEK_END) < 0) return false;
    return true;
}

bool WAL::writeAll(int fd, const void* p, size_t n) {
    const char* c = static_cast<const char*>(p);
    size_t left = n;
    while (left) {
        ssize_t w = ::write(fd, c, left);
        if (w < 0) return false;
        c += w;
        left -= w;
    }
    return true;
}
bool WAL::readAll(int fd, void* p, size_t n) {
    char* c = static_cast<char*>(p);
    size_t left = n;
    while (left) {
        ssize_t r = ::read(fd, c, left);
        if (r == 0) return false;  // EOF before we got everything
        if (r < 0) return false;
        c += r;
        left -= r;
    }
    return true;
}

bool WAL::ensureOpenForWrite() {
    if (fd_ >= 0) return true;
    return open();
}

bool WAL::writeRecord(const std::string& key, RecType t, const std::string* val) {
    if (!ensureOpenForWrite()) return false;

    uint32_t klen = static_cast<uint32_t>(key.size());
    uint32_t vlen = (t == RecType::Put && val) ? static_cast<uint32_t>(val->size()) : 0;

    // Build a buffer to compute CRC32
    std::string buf;
    buf.append(reinterpret_cast<const char*>(&klen), sizeof(klen));
    buf.append(key);
    buf.push_back(static_cast<char>(t));
    buf.append(reinterpret_cast<const char*>(&vlen), sizeof(vlen));
    if (val && vlen) buf.append(*val);

    uint32_t crc = compute_crc32(buf);

    // Write the actual record
    if (!writeU32(fd_, klen)) return false;
    if (!writeAll(fd_, key.data(), klen)) return false;
    if (!writeU8(fd_, static_cast<uint8_t>(t))) return false;
    if (!writeU32(fd_, vlen)) return false;
    if (vlen && !writeAll(fd_, val->data(), vlen)) return false;
    if (!writeU32(fd_, crc)) return false;

    return true;
}

bool WAL::appendPut(const std::string& key, const std::string& value) {
    return writeRecord(key, RecType::Put, &value);
}

bool WAL::appendDel(const std::string& key) {
    return writeRecord(key, RecType::Del, nullptr);
}

bool WAL::sync() {
    if (fd_ < 0) return true;
    return ::fsync(fd_) == 0;
}

bool WAL::replay(MemTable& mem) {
    // Open read-only and scan from start.
    int rfd = ::open(path_.c_str(), O_RDONLY);
    if (rfd < 0) return false;

    // Verify header
    uint32_t m = 0, v = 0;
    if (!readU32(rfd, m) || !readU32(rfd, v) || m != kMagic || v != kVersion) {
        ::close(rfd);
        return false;
    }

    // Read records until EOF or incomplete tail
    while (true) {
        uint32_t klen = 0, vlen = 0, crc_stored = 0;
        uint8_t type = 0;

        // Read key length
        ssize_t got = ::read(rfd, &klen, sizeof(klen));
        if (got == 0) break;  // clean EOF
        if (got != (ssize_t)sizeof(klen)) {
            ::close(rfd);
            return true;
        }

        std::string key(klen, '\0');
        if (!readAll(rfd, key.data(), klen)) {
            ::close(rfd);
            return true;
        }

        if (!readU8(rfd, type)) {
            ::close(rfd);
            return true;
        }
        if (!readU32(rfd, vlen)) {
            ::close(rfd);
            return true;
        }

        std::string val;
        if (type == (uint8_t)RecType::Put) {
            val.resize(vlen);
            if (!readAll(rfd, val.data(), vlen)) {
                ::close(rfd);
                return true;
            }
        } else if (vlen) {
            std::vector<char> skip(vlen);
            if (!readAll(rfd, skip.data(), vlen)) {
                ::close(rfd);
                return true;
            }
        }

        // Read checksum
        if (!readU32(rfd, crc_stored)) {
            ::close(rfd);
            return true;
        }

        // Build buffer for validation
        std::string buf;
        buf.append(reinterpret_cast<const char*>(&klen), sizeof(klen));
        buf.append(key);
        buf.push_back(type);
        buf.append(reinterpret_cast<const char*>(&vlen), sizeof(vlen));
        if (type == (uint8_t)RecType::Put) {
            buf.append(val);
        } else if (vlen) {
            buf.append(std::string(vlen, '\0'));  // filler to preserve CRC format
        }

        uint32_t crc_expected = compute_crc32(buf);
        if (crc_expected != crc_stored) {
            std::cerr << "WAL: checksum mismatch. Skipping corrupt record.\n";
            continue;
        }

        // Apply to MemTable
        if (type == (uint8_t)RecType::Put) {
            mem.put(std::move(key), std::move(val));
        } else if (type == (uint8_t)RecType::Del) {
            mem.del(std::move(key));
        } else {
            std::cerr << "WAL: unknown record type. Aborting replay.\n";
            break;
        }
    }

    ::close(rfd);
    return true;
}

bool WAL::reset() {
    // Truncate file to just the header (keep magic/version)
    int tfd = ::open(path_.c_str(), O_RDWR | O_TRUNC, 0644);
    if (tfd < 0) return false;
    // Write header
    if (!writeU32(tfd, kMagic) || !writeU32(tfd, kVersion)) {
        ::close(tfd);
        return false;
    }
    ::close(tfd);
    // Re-open append fd_ positioned at end
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    return open();
}