#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <string>
#include <iomanip>
#include <filesystem>

#include "utils.hpp"
#include "test_utils.hpp"

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
    std::pair<std::string, std::string> parsePath(const std::string &uri) 
    {
        std::string cleanUri = uri;

        // remove ending '/', if present
        if (cleanUri[cleanUri.size() - 1] == '/')
            cleanUri = cleanUri.substr(0, cleanUri.size() - 1);

        size_t lastSlashPos = cleanUri.find_last_of('/');

        if (lastSlashPos == std::string::npos)
            return {cleanUri, ""};
        
        if (lastSlashPos == 0)
            return {cleanUri, ""};

        std::string prefix = cleanUri.substr(0, lastSlashPos);
        std::string key = cleanUri.substr(lastSlashPos + 1);

        return {prefix.empty() ? cleanUri : prefix, key};
    } 
};

namespace PrintUtils {

    // template<typename T>
    // std::string printVector(const std::vector<T>& vec) 
    // {
    //     std::ostringstream oss;
    //     oss << "[ ";
    //     for (size_t i = 0; i < vec.size(); ++i) 
    //     {
    //         oss << vec[i];
    //         if (i != vec.size() - 1) {
    //             oss << ", ";
    //         }
    //     }
    //     oss << " ]";
    //     return oss.str();
    // }

    /**
     * Prints given 32-bit integer in hex form
     */
    void printHex32(uint32_t value) 
    {
        std::cout << std::hex << std::setw(8) << std::setfill('0') 
                  << value
                  << std::endl;
    }

    /**
     * Pads `text` such that it sits in the center of a 
     * new text string of width `width`.
     */
    std::string centerText(std::string text, int width) 
    {
        size_t size = text.size(); 

        if (size > static_cast<size_t>(width)) 
            return text.substr(0, width);

        int padding = (width - size) / 2;
        int extra = (width - size) % 2; // handle odd padding
        return std::string(padding, ' ') + text + std::string(padding + extra, ' ');
    }

    /**
     * Returns compact string representation of the number of
     * bytes `bytes`, using traditional suffixes KB/MB/GB/TB etc.
     */
    std::string formatNumBytes(uint64_t bytes)
    {
        const char* suffixes[] = {" bytes", "KB", "MB", "GB", "TB", "PB"};
        int suffixIndex = 0;
        double size = static_cast<double>(bytes);

        while (size >= 1024 && suffixIndex < 5)
        {
            size /= 1024;
            suffixIndex++;
        }

        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << size << " " << suffixes[suffixIndex];
        return oss.str();
    }
}

namespace StringUtils
{
    /**
     * Returns new copy of `s`, truncated/padded to be
     * of size `size`.
     */
    std::string fixedSize(std::string s, uint32_t size)
    {
        s.resize(size, '\0');
        return s;
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

////////////////////////////////////////////
// Utils tests
////////////////////////////////////////////
namespace UtilsTests
{
    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "Utils Tests" << std::endl;
        std::cerr << "###################################" << std::endl;

        std::vector<std::pair<std::string, std::function<void()>>> tests = {
            TEST(testParsePath)
        };

        for (auto &[name, func] : tests)
        {
            TestUtils::runTest(name, func);
        }
    }

    void testParsePath()
    {
        std::vector<std::string> paths = {
            "/store/archive.zip",
            "/store/archive.zip/",

            "/keys",
            "/keys/"
        };

        std::vector<std::pair<std::string, std::string>> expectedParsedPaths = {
            {"/store", "archive.zip"},
            {"/store", "archive.zip"},

            {"/keys", ""},
            {"/keys", ""}
        };

        for (int i = 0; i < paths.size(); i++)
        {
            auto p = ApiUtils::parsePath(paths[i]);
            ASSERT_THAT(p == expectedParsedPaths[i]);
        }
    }
};