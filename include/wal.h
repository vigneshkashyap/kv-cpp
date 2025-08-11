#pragma once
#include <cstdint>
#include <string>
#include "memtable.h"

class WAL {
public:
    explicit WAL(std::string path);
    ~WAL();

    bool open();
    bool appendPut(const std::string&key, const std::string& value);
    bool appendDel(const std::string& key);

    bool sync();

    bool replay(MemTable& memtable);

    bool reset();

private:
    std::string path_;
    int fd_ = -1;

    bool ensureOpenForWrite();
    bool writeRecord(const std::string& key, RecType t, const std::string* val);
    static bool writeAll(int fd, const void* p, size_t n);
    static bool readAll(int fd, void* p, size_t n);
    bool validateHeader();
};