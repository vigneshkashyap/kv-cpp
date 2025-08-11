#include "memtable.h"
#include "wal.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <system_error>
#include <fcntl.h>
#include <unistd.h>


namespace fs = std::filesystem;

static void clean_dir(const fs::path& p) {
    std::error_code ec;
    fs::remove_all(p, ec);
    fs::create_directories(p, ec);
    (void)ec;
}

static size_t local_file_size(const fs::path& p) {
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    return ec ? 0 : static_cast<size_t>(sz);
}

static void truncate_bytes_from_end(const fs::path& p, size_t bytes) {
    // POSIX ftruncate
    std::error_code ec;
    auto sz = fs::file_size(p, ec);
    if (ec || sz == 0 || sz <= bytes) return;
#ifdef _WIN32
    // Windows fallback: rewrite to smaller size
    std::ifstream in(p, std::ios::binary);
    std::string buf;
    buf.resize(static_cast<size_t>(sz - bytes));
    in.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    in.close();
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    out.close();
#else
    int fd = ::open(p.c_str(), O_RDWR);
    if (fd >= 0) {
        ::ftruncate(fd, static_cast<off_t>(sz - bytes));
        ::close(fd);
    }
#endif
}

static std::string rand_string(size_t n) {
    static thread_local std::mt19937_64 rng(
        std::chrono::high_resolution_clock::now().time_since_epoch().count());
    static const char alphabet[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::uniform_int_distribution<size_t> dist(0, sizeof(alphabet) - 2);
    std::string s; s.reserve(n);
    for (size_t i = 0; i < n; ++i) s.push_back(alphabet[dist(rng)]);
    return s;
}

static void test_header_new_and_existing() {
    std::cout << "[T] header_new_and_existing\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    {
        WAL wal(walp.string());
        assert(wal.open());
        // New file should be just header (8 bytes)
        assert(local_file_size(walp) == 8);
    }
    {
        WAL wal(walp.string());
        assert(wal.open()); // existing should validate and leave size unchanged
        assert(local_file_size(walp) == 8);
    }
}

static void test_happy_path_replay() {
    std::cout << "[T] happy_path_replay\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    WAL wal(walp.string());
    assert(wal.open());
    assert(wal.appendPut("a", "1"));
    assert(wal.appendPut("b", "2"));
    assert(wal.appendPut("a", "3"));
    assert(wal.appendDel("b"));
    assert(wal.sync());

    MemTable mem;
    WAL rdr(walp.string());
    assert(rdr.open());
    assert(rdr.replay(mem));

    auto a = mem.get("a");
    assert(a && a->type == RecType::Put && a->value == "3");
    auto b = mem.get("b");
    assert(b && b->type == RecType::Del);
}

static void test_truncated_tail_tolerance() {
    std::cout << "[T] truncated_tail_tolerance\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    // Write a few records
    {
        WAL wal(walp.string());
        assert(wal.open());
        for (int i = 0; i < 10; ++i) {
            assert(wal.appendPut("k" + std::to_string(i), "v" + std::to_string(i)));
        }
        // start a final record we will later truncate
        assert(wal.appendPut("incomplete", "xxxxxxxxxxxxxxxxxxxxxxxx"));
        assert(wal.sync());
    }

    // Chop off 7 bytes from end (mid-record)
    truncate_bytes_from_end(walp, 7);

    // Replay should succeed and reconstruct complete records only
    MemTable mem;
    WAL rdr(walp.string());
    assert(rdr.open());
    assert(rdr.replay(mem));

    for (int i = 0; i < 10; ++i) {
        auto v = mem.get("k" + std::to_string(i));
        assert(v && v->type == RecType::Put && v->value == "v" + std::to_string(i));
    }
    // The incomplete record may or may not be present; it should NOT crash or corrupt.
}

static void test_reset() {
    std::cout << "[T] reset\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    WAL wal(walp.string());
    assert(wal.open());
    assert(wal.appendPut("x", "y"));
    assert(wal.sync());
    assert(file_size(walp) > 8);

    assert(wal.reset());
    assert(file_size(walp) == 8);

    // Appends after reset still work and replay reads only new records
    assert(wal.appendPut("a", "1"));
    assert(wal.sync());

    MemTable mem;
    WAL rdr(walp.string());
    assert(rdr.open());
    assert(rdr.replay(mem));
    auto a = mem.get("a");
    assert(a && a->type == RecType::Put && a->value == "1");
    auto x = mem.get("x");
    assert(!x.has_value());
}

static void test_idempotent_replay() {
    std::cout << "[T] idempotent_replay\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    WAL wal(walp.string());
    assert(wal.open());
    assert(wal.appendPut("user:1", "Alice"));
    assert(wal.appendPut("user:1", "Alicia"));
    assert(wal.appendDel("user:2"));
    assert(wal.sync());

    // Replay #1
    MemTable m1;
    WAL r1(walp.string());
    assert(r1.open());
    assert(r1.replay(m1));

    // Replay #2
    MemTable m2;
    WAL r2(walp.string());
    assert(r2.open());
    assert(r2.replay(m2));

    auto v1 = m1.get("user:1");
    auto v2 = m2.get("user:1");
    assert(v1 && v2 && v1->value == "Alicia" && v2->value == "Alicia");

    auto d1 = m1.get("user:2");
    auto d2 = m2.get("user:2");
    assert(d1 && d2 && d1->type == RecType::Del && d2->type == RecType::Del);
}

static void test_large_keys_values() {
    std::cout << "[T] large_keys_values\n";
    clean_dir("testdata");
    const fs::path walp = "testdata/wal.log";

    std::string bigK = rand_string(64 * 1024);     // 64KB key
    std::string bigV = rand_string(256 * 1024);    // 256KB value

    WAL wal(walp.string());
    assert(wal.open());
    assert(wal.appendPut(bigK, bigV));
    assert(wal.sync());

    MemTable mem;
    WAL rdr(walp.string());
    assert(rdr.open());
    assert(rdr.replay(mem));

    auto v = mem.get(bigK);
    assert(v && v->type == RecType::Put && v->value == bigV);
}

int main() {
    test_header_new_and_existing();
    test_happy_path_replay();
    test_truncated_tail_tolerance();
    test_reset();
    test_idempotent_replay();
    test_large_keys_values();

    std::cout << "All WAL tests passed ✅\n";
    return 0;
}
