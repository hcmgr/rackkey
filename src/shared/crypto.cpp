#include <openssl/sha.h>
#include <string>

#include "crypto.hpp"

namespace Crypto {

    #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    /**
     * Computes 32-bit truncation of the given input's SHA256 hash
     */
    uint32_t sha256_32(const std::string& input) {
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;

        SHA256_Init(&sha256);
        SHA256_Update(&sha256, input.c_str(), input.size());
        SHA256_Final(hash, &sha256);

        // extract first 4 bytes (32 bits)
        uint32_t hashValue = (uint32_t(hash[0]) << 24) | 
                             (uint32_t(hash[1]) << 16) | 
                             (uint32_t(hash[2]) << 8)  | 
                             (uint32_t(hash[3]));

        return hashValue;
    }
}