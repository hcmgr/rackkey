#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

int main() {
    const std::string apiPath = "https://www.example.com";
    const std::string outputFilePath = "out/test.html";

    std::shared_ptr<ostream> fileStream = std::make_shared<ostream>();

    /**
     * Build HTTP get task
     */
    pplx::task<void> simpleGetTask = fstream::open_ostream(U(outputFilePath)).then([=](ostream stream){
        *fileStream = stream;
    })
    // make request
    .then([=](void) {
        http_client client(U(apiPath));
        return client.request(methods::GET);
    })
    // process response
    .then([=](http_response response) {
        printf("Received response status code: %u\n", response.status_code());
        return response.body().read_to_end(fileStream->streambuf());
    })
    // cleanup
    .then([=](size_t bytesWritten) {
        fileStream->close();
        return;
    });

    // start GET task
    try {
        simpleGetTask.wait();
    } catch (const std::exception &e) {
        printf("Exception: %s\n", e.what());
    }

    return 0;
}
