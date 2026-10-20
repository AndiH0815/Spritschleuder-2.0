#pragma once
// Host adapter only. Production links the ESP-IDF PSA crypto implementation.
#include <cstdint>
#include <openssl/evp.h>
#include <string>
using psa_status_t = int;
constexpr int PSA_SUCCESS = 0, PSA_ALG_SHA_256 = 1, PSA_KEY_DERIVATION_INPUT_COST = 1,
              PSA_KEY_DERIVATION_INPUT_SALT = 2, PSA_KEY_DERIVATION_INPUT_PASSWORD = 3;
struct psa_key_derivation_operation_t {
    std::string salt, password;
    int cost = 0;
};
#define PSA_KEY_DERIVATION_OPERATION_INIT                                                          \
    {                                                                                              \
    }
inline int PSA_ALG_PBKDF2_HMAC(int) { return 1; }
inline int psa_crypto_init() { return 0; }
inline int psa_key_derivation_setup(psa_key_derivation_operation_t *, int) { return 0; }
inline int psa_key_derivation_input_integer(psa_key_derivation_operation_t *o, int, int c) {
    o->cost = c;
    return 0;
}
inline int psa_key_derivation_input_bytes(psa_key_derivation_operation_t *o, int k,
                                          const unsigned char *p, size_t n) {
    (k == PSA_KEY_DERIVATION_INPUT_SALT ? o->salt : o->password)
        .assign(reinterpret_cast<const char *>(p), n);
    return 0;
}
inline int psa_key_derivation_output_bytes(psa_key_derivation_operation_t *o, unsigned char *p,
                                           size_t n) {
    return PKCS5_PBKDF2_HMAC(o->password.data(), int(o->password.size()),
                             reinterpret_cast<const unsigned char *>(o->salt.data()),
                             int(o->salt.size()), o->cost, EVP_sha256(), int(n), p) == 1
               ? 0
               : -1;
}
inline void psa_key_derivation_abort(psa_key_derivation_operation_t *) {}
