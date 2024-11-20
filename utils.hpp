#pragma once

#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace ApiUtils {
    json::value createPlaceholderJson();
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
            std::cout << static_cast<int>(vec[i]);
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