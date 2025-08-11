#include "memtable.h"
#include <cassert>
#include <iostream>

int main() {
    MemTable m;

    m.put("a", "1");
    m.put("b", "2");
    m.put("a", "3"); // overwrite
    m.del("b");      // tombstone

    auto a = m.get("a");
    assert(a && a->type == RecType::Put && a->value == "3");

    auto b = m.get("b");
    assert(b && b->type == RecType::Del);

    std::cout << "approxBytes=" << m.approxBytes() << "\n";
    std::cout << "MemTable OK\n";
}
