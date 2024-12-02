#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <string>
#include <iomanip>

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