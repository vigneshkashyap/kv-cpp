#include "memtable.h"
#include "wal.h"
#include <cassert>
#include <iostream>
#include <filesystem>

int main() {
    namespace fs = std::filesystem;
    fs::create_directories("data");

    MemTable m;
    WAL wal("data/wal.log");
    assert(wal.open());

    assert(wal.appendPut("a", "1"));
    assert(wal.appendPut("b", "2"));
    assert(wal.appendPut("a", "3"));
    assert(wal.appendDel("b"));
    assert(wal.sync());

    MemTable recovered;
    WAL reader("data/wal.log");
    assert(reader.open());
    assert(reader.replay(recovered));

    auto a = recovered.get("a");
    assert(a && a->type == RecType::Put && a->value == "3");

    auto b = recovered.get("b");
    assert(b && b->type == RecType::Del);

    std::cout << "WAL replay OK\n";
}
