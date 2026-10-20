#pragma once
#include <cstddef>
#include <openssl/rand.h>
inline void esp_fill_random(void *p, size_t n) {
    RAND_bytes(static_cast<unsigned char *>(p), int(n));
}
