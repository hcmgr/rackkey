#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <string>
#include <iostream>

#include "utils.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

void getHandler(http_request request)
{
    // send response
    json::value responseJson = ApiUtils::createPlaceholderJson();
    http_response response(status_codes::OK);
    response.set_body(responseJson);
    request.reply(response);
}


void startServer() 
{
    uri_builder uri("http://localhost:8081");
    auto addr = uri.to_uri().to_string();
    http_listener listener(addr);

    listener.support([](http_request request) {
        getHandler(request);
    });

    try {
        listener
            .open()
            .then([&addr](){ std::cout << "Server is listening at: " << addr << std::endl; });
        while (1);
    } catch (const std::exception& e) {
        std::cout << "An error occurred: " << e.what() << std::endl;
    }
}

int main()
{
    startServer();
}