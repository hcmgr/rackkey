#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

namespace Utils {
    json::value createPlaceholderJson() {
        json::value responseJson;
        responseJson[U("message")] = json::value::string(U("Hello"));
        responseJson[U("status")] = json::value::number(200);
        return responseJson;
    }
};
