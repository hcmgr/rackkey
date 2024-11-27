#include <openssl/sha.h>
#include <string>

namespace Crypto {

    /**
     * Computes 32-bit truncation of the given input's SHA256 hash
     */
    uint32_t sha256_32(const std::string& input);
}