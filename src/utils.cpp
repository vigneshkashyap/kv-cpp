#include "utils.h"

#include <zlib.h>

uint32_t compute_crc32(const std::string& data) {
    return crc32(0L, reinterpret_cast<const unsigned char*>(data.data()), data.size());
}
