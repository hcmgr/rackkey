#pragma once

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <cstdint>
#include <filesystem>
#include <unordered_set>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace fs = std::filesystem;

namespace ApiUtils {

    /**
     * Creates a json response object with a single http status field
     */
    json::value statusResponse(status_code code);

    /**
     * Utility function to parse the given uri into a {path, param} pair.
     * 
     * E.g.
     * 
     * "/store/key1" OR "/store/key1/" -> {"/store", "key1"}
     * 
     * E.g.
     * 
     * "/keys" OR "/keys/" -> {"/keys", ""}
     */
    std::pair<std::string, std::string> parsePath(const std::string &uri);
};
    
namespace PrintUtils {

    /**
     * Pretty-print std::vector<T>
     */
    template<typename T>
    void printVector(const std::vector<T>& vec) 
    {
        std::cout << "[ ";
        for (size_t i = 0; i < vec.size(); ++i) 
        {
            std::cout << vec[i];
            if (i != vec.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << " ]" << std::endl;
    }

    /**
     * Pretty-print std::unordered_set<T>
     */
    template<typename T>
    void printUnorderedSet(const std::unordered_set<T>& set) 
    {
        std::cout << "{ ";
        int cnt = 0;
        for (auto &el : set)
        {
            std::cout << el;
            if (cnt++ != set.size() - 1)
                std::cout << ", ";
        }
        std::cout << " }" << std::endl;
    }

    /**
     * Pretty-print std::map<K, V>
     */
    template <typename K, typename V>
    void printMap(const std::map<K, V>& m) 
    {
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

namespace FileSystemUtils
{
    /**
     * Remove all contents of given directory, and the 
     * directory itself.
     */
    void removeDirectory(fs::path dirPath);
}