#include "engine.h"
#include <iostream>
#include <sstream>
#include <string>

static void print_help() {
    std::cout << 
        "Commands:\n"
        "  PUT <key> <value...>\n"
        "  GET <key>\n"
        "  DEL <key>\n"
        "  SNAPSHOT\n"
        "  EXIT\n";
}

int main(int argc, char** argv) {
    std::string data_dir = "mini_kv_data";
    if (argc >= 2) data_dir = argv[1];

    Engine db(data_dir);
    std::cout << "kvx-mini ready in '" << data_dir << "'. Type HELP for commands.\n";
    print_help();

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;

        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        if (cmd == "PUT") {
            std::string key;
            iss >> key;
            std::string value;
            std::getline(iss, value);
            if (!value.empty() && value[0] == ' ') value.erase(0,1);
            if (key.empty()) {
                std::cout << "ERR: PUT needs <key> <value>\n";
                continue;
            }
            db.put(key, value);
            std::cout << "OK\n";
        } else if (cmd == "GET") {
            std::string key;
            iss >> key;
            if (key.empty()) { std::cout << "ERR: GET needs <key>\n"; continue; }
            auto v = db.get(key);
            if (v.empty() && !db.state().count(key)) {
                std::cout << "(nil)\n";
            } else {
                std::cout << v << "\n";
            }
        } else if (cmd == "DEL") {
            std::string key;
            iss >> key;
            if (key.empty()) { std::cout << "ERR: DEL needs <key>\n"; continue; }
            db.del(key);
            std::cout << "OK\n";
        } else if (cmd == "SNAPSHOT") {
            db.snapshot();
            std::cout << "Snapshot complete. WAL truncated.\n";
        } else if (cmd == "HELP") {
            print_help();
        } else if (cmd == "EXIT" || cmd == "QUIT") {
            break;
        } else {
            std::cout << "Unknown command. Type HELP.\n";
        }
    }
    return 0;
}
