#pragma once

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <cstdint>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace ApiUtils {

    /**
     * Creates a json response object with a single http status field
     */
    json::value statusResponse(status_code code);

    /**
     * Splits the api path into a prefix and final parameter.
     * 
     * E.g.
     * "/api/store/archive.zip" - returns {"/api/store", "archive.zip"}
     * 
     * E.g.
     * "/add/node1" - returns {"/add", "node1"}
     */
    std::pair<std::string, std::string> splitApiPath(const std::string& relPath);
};
    
namespace PrintUtils {

    /**
     * Pretty-print std::vector<T>
     */
    template<typename T>
    void printVector(const std::vector<T>& vec) 
    {
        std::cout << "[ ";
        for (size_t i = 0; i < vec.size(); ++i) {
            std::cout << static_cast<char>(vec[i]);
            if (i != vec.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << " ]" << std::endl;
    }

    /**
     * Pretty-print std::map<K, V>
     */
    template <typename K, typename V>
    void printMap(const std::map<K, V>& m) {
        std::cout << "{\n";
        for (const auto& pair : m) {
            std::cout << "  " << pair.first << ": " << pair.second << "\n";
        }
        std::cout << "}\n";
    }

    /**
     * Prints given 32-bit integer in hex form
     */
    void printHex32(uint32_t value);
};

namespace MathUtils
{
    /**
     * Returns ceiling of integer division of `numerator` and `denominator`.
     * 
     * e.g. 7 / 3 => 3
     */
    uint32_t ceilDiv(uint32_t numerator, uint32_t denominator);
}

namespace VectorUtils
{
    std::vector<unsigned char> flatten(std::vector<std::vector<unsigned char>>& vecs);
}
