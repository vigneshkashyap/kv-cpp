#include "engine.h"
#include <iostream>
#include <sstream>

static void help() {
    std::cout
      << "Commands:\n"
      << "  put <key> <value...>\n"
      << "  get <key>\n"
      << "  del <key>\n"
      << "  flush           # force flush MemTable -> SSTable\n"
      << "  list            # list SSTables\n"
      << "  sync            # fsync WAL\n"
      << "  stats           # mem size/bytes\n"
      << "  help\n"
      << "  exit | quit\n";
}

int main() {
    Engine db("data", /*flush threshold*/ 256 * 1024); // 256KB for easy testing
    if (!db.open()) {
        std::cerr << "Failed to open engine\n";
        return 1;
    }

    std::cout << "KV REPL ready. Type 'help'.\n";
    std::string line;
    while (std::cout << "> " && std::getline(std::cin, line)) {
        std::istringstream iss(line);
        std::string cmd; iss >> cmd;
        if (cmd.empty()) continue;

        if (cmd == "help") { help(); continue; }
        if (cmd == "exit" || cmd == "quit") break;

        if (cmd == "put") {
            std::string key; iss >> key;
            std::string value;
            std::getline(iss, value);
            if (!value.empty() && value[0] == ' ') value.erase(0,1);
            if (key.empty()) { std::cout << "usage: put <key> <value>\n"; continue; }
            if (!db.put(key, value)) std::cout << "ERR\n"; else std::cout << "OK\n";
            continue;
        }
        if (cmd == "get") {
            std::string key; iss >> key;
            if (key.empty()) { std::cout << "usage: get <key>\n"; continue; }
            auto v = db.get(key);
            if (v) std::cout << *v << "\n"; else std::cout << "(nil)\n";
            continue;
        }
        if (cmd == "del") {
            std::string key; iss >> key;
            if (key.empty()) { std::cout << "usage: del <key>\n"; continue; }
            if (!db.del(key)) std::cout << "ERR\n"; else std::cout << "OK\n";
            continue;
        }
        if (cmd == "flush") {
            if (!db.flush()) std::cout << "ERR\n"; else std::cout << "OK\n";
            continue;
        }
        if (cmd == "list") { db.list_tables(); continue; }
        if (cmd == "sync") { std::cout << (db.sync() ? "OK\n" : "ERR\n"); continue; }
        if (cmd == "stats") {
            std::cout << "mem.size=" << db.mem_size() << " mem.bytes=" << db.mem_bytes() << "\n";
            continue;
        }

        std::cout << "unknown: " << cmd << " (try 'help')\n";
    }
}
