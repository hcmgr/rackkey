#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

#include "utils.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

class MasterServer {
private:


public:
    MasterServer() {

    }

    ~MasterServer() {

    }
};

/**
 * GET
 * ---
 * Given: (KEY)
 * Requests all blocks for the given KEY from the storage cluster, returns in order
 */
void getHandler(http_request request) {
    std::cout << "get req received" << std::endl;
    
    json::value responseJson = ApiUtils::createPlaceholderJson();
    
    http_response response(status_codes::OK);
    response.set_body(responseJson);
    
    request.reply(response);
}

/**
 * PUT
 * ---
 * Given: (KEY, payload)
 * Breaks payload into blocks and distributes them across the storage cluster
 */
void putHandler(http_request request) {
    std::cout << "put req received" << std::endl;

    json::value responseJson = ApiUtils::createPlaceholderJson();
    
    http_response response(status_codes::OK);
    response.set_body(responseJson);
    
    request.reply(response);
}

/**
 * DELETE
 * ---
 * Given: (KEY)
 * Deletes all blocks of the given KEY from the storage cluster
 */
void deleteHandler(http_request request) {
    std::cout << "del req received" << std::endl;

    json::value responseJson = ApiUtils::createPlaceholderJson();
    
    http_response response(status_codes::OK);
    response.set_body(responseJson);
    
    request.reply(response);
}

int main() {
    uri_builder uri(U("http://localhost:8082"));
    auto addr = uri.to_uri().to_string();
    http_listener listener(addr);

    listener.support(methods::GET, getHandler);
    listener.support(methods::POST, putHandler);
    listener.support(methods::DEL, deleteHandler);

    try {
        listener
            .open()
            .then([&addr](){ std::cout << "Server is listening at: " << addr << std::endl; });
        while (1);
    } catch (const std::exception& e) {
        std::cout << "An error occurred: " << e.what() << std::endl;
    }
    return 0;
}