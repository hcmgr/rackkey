#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>

using namespace web;
using namespace web::http;
using namespace web::http::experimental::listener;

void getKeyFromRequestPath(http_request request) {
    auto relativePath = uri::decode(request.relative_uri().path());
    if (relativePath.empty() || relativePath == U("/")) {
        request.reply(status_codes::BadRequest, "Filename not provided in the request URL");
        return;
    }
    
    if (relativePath[0] == '/') {
        relativePath = relativePath.substr(1);
    }
}

/**
 * EXAMPLE
 * ----
 * Serve a GET request for a large zip file
 */
void example_handle_get_file(http_request request) {
    std::cout << "received req" << std::endl;
    const std::string filePath = "../example_files/archive.zip";

    // Open the file in binary mode
    std::ifstream fileStream(filePath, std::ios::binary | std::ios::ate);
    if (!fileStream) {
        std::cerr << "Error: Could not open file at path: " << filePath << std::endl;
        perror("Reason");
        request.reply(status_codes::NotFound, "File not found");
        return;
    }

    // Get the file size
    std::streamsize fileSize = fileStream.tellg();
    fileStream.seekg(0, std::ios::beg);

    // Read the file into a buffer
    std::vector<uint8_t> buffer(fileSize);
    if (!fileStream.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
        std::cout << "Failed to read file: " << filePath << std::endl;
        request.reply(status_codes::InternalError, "Failed to read file");
        return;
    }

    // Send the file as a response
    http_response response(status_codes::OK);
    response.set_body(buffer);

    request.reply(response);
    std::cout << "Served file: " << filePath << std::endl;
}


int main() {
    uri_builder uri(U("http://localhost:8082"));
    auto addr = uri.to_uri().to_string();
    http_listener listener(addr);

    listener.support(methods::GET, example_handle_get_file);
    std::cout << status_codes::OK << std::endl;

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
