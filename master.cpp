#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

/**
 * GET
 * ---
 * Given: (KEY)
 * Requests all blocks for the given KEY, returns in order
 */
void getHandler(http_request request) {
    std::cout << "get req received" << std::endl;
    http_response response(status_codes::OK);

    // std::vector<uint8_t> buffer(10);
    // for (int i = 0; i < 10; i++) {
    //     buffer[i] = i;
    // }
    // response.set_body(buffer);

    request.reply(response);
}

void putHandler(http_request request) {
    std::cout << "put req received" << std::endl;
    http_response response(status_codes::OK);
    request.reply(response);
}

void deleteHandler(http_request request) {
    std::cout << "del req received" << std::endl;
    http_response response(status_codes::OK);
    request.reply(response);
}

int main() {
    uri_builder uri(U("http://localhost:8080"));
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