#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace ApiUtils {
    json::value createPlaceholderJson() 
    {
        json::value responseJson;
        responseJson[U("message")] = json::value::string(U("Hello"));
        responseJson[U("status")] = json::value::number(200);
        return responseJson;
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