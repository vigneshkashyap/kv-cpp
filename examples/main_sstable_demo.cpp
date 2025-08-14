#include "sstable.h"
#include <iostream>
#include <cassert>

int main() {
    // Prepare sorted, unique entries
    std::vector<std::pair<std::string, MemValue>> entries = {
        {"a", MemValue{RecType::Put, "1"}},
        {"b", MemValue{RecType::Put, "2"}},
        {"c", MemValue{RecType::Del, ""}},
        {"d", MemValue{RecType::Put, "4"}}
    };
    // Keys must be strictly increasing (a < b < c < d)

    std::string out_path;
    bool ok = SSTable::Build("data", /*file_id=*/1, entries, &out_path);
    assert(ok);
    std::cout << "Built: " << out_path << "\n";

    SSTable sst;
    assert(sst.Open(out_path));

    auto va = sst.Get("a");  assert(va && *va == "1");
    auto vb = sst.Get("b");  assert(vb && *vb == "2");
    auto vc = sst.Get("c");  assert(!vc);           // tombstone -> not found
    auto vx = sst.Get("x");  assert(!vx);           // absent
    std::cout << "SSTable lookups OK\n";
}
