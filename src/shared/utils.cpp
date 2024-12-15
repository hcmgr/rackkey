#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>

#include "utils.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace ApiUtils {
    /**
     * Creates a json response object with a single http status field
     */
    json::value statusResponse(status_code code) 
    {
        json::value responseJson;
        responseJson[U("status")] = json::value::number(code);
        return responseJson;
    }

    /**
     * Splits the api path into a prefix and final parameter.
     * 
     * E.g.
     * "/api/store/archive.zip" - returns {"/api/store/", "archive.zip"}
     * 
     * E.g.
     * "/store/node1" - returns {"/store/", "node1"}
     * 
     * E.g.
     * "/something.csv" - returns {"/", "something.csv"}
     */
    std::pair<std::string, std::string> splitApiPath(const std::string& relPath)
    {
        size_t lastSlashPos = relPath.find_last_of('/');
        if (lastSlashPos == std::string::npos || lastSlashPos == relPath.size() - 1) 
            return {};
        return {relPath.substr(0, lastSlashPos + 1) , relPath.substr(lastSlashPos + 1)};
    }
};

namespace PrintUtils {
    /**
     * Prints given 32-bit integer in hex form
     */
    void printHex32(uint32_t value) 
    {
        std::cout << std::hex << std::setw(8) << std::setfill('0') 
                  << value
                  << std::endl;
    }
}

namespace MathUtils
{
    /**
     * Returns ceiling of integer division of `numerator` and `denominator`.
     * 
     * e.g. 7 / 3 => 3
     */
    uint32_t ceilDiv(uint32_t numerator, uint32_t denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }
}

namespace VectorUtils
{
    std::vector<unsigned char> flatten(std::vector<std::vector<unsigned char>>& vecs) 
    {
        std::vector<unsigned char> result;
        for (auto& vec: vecs) 
        {
            result.insert(result.end(), vec.begin(), vec.end());
        }
        return result;
    }
}

namespace FileSystemUtils
{
    /**
     * Remove all contents of given directory, and the 
     * directory itself.
     */
    void removeDirectory(fs::path dirPath)
    {
        if (fs::exists(dirPath))
            fs::remove_all(dirPath);
    }
}