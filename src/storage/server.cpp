#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <string>
#include <iostream>
#include <cstdlib>

#include "block.hpp"
#include "utils.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class StorageServer
{
private:
    /**
     * Placeholder for the actual on-disk storage.
     */
    std::map<std::string, std::vector<Block>> blockStore;

public:

    /**
     * Default constructor
     */
    StorageServer()
        : blockStore()
    {
    }

    /**
     * Retreives blocks of the given `key` from storage.
     */
    void getHandler(http_request request, std::string key)
    {
        std::cout << "GET req received: " << key << std::endl;

        // TODO: retreive from actual store
        std::vector<Block> blocks = this->blockStore[key];

        // serialize
        std::vector<unsigned char> payloadBuffer;
        for (auto block : blocks)
        {
            block.serialize(payloadBuffer);
        }

        // build and send response
        http_response response(status_codes::OK);
        response.set_body(payloadBuffer);
        request.reply(response);
    }

    /**
     * Writes given blocks to storage for the given `key`.
     */
    void putHandler(http_request request, std::string key)
    {
        std::cout << "PUT req received: " << key << std::endl;
        std::shared_ptr<std::vector<unsigned char>> payloadPtr = std::make_shared<std::vector<unsigned char>>();

        pplx::task<void> task = request.extract_vector()

        // extract list of blocks
        .then([&](std::vector<unsigned char> payload)
        {
            *payloadPtr = std::move(payload);

            std::vector<Block> blocks = Block::deserialize(*payloadPtr);

            for (auto block : blocks)
            {
                // TODO: write to actual storage
                this->blockStore[key].push_back(block);
            }
        });

        task.wait();

        request.reply(status_codes::OK);
        return;
    }
    
    /**
     * 
     */
    void deleteHandler(http_request request, std::string key)
    {

    }

    void startServer() 
    {
        uri_builder uri("http://localhost:8081");
        auto addr = uri.to_uri().to_string();
        http_listener listener(addr);

        listener.support([this](http_request request) {
            this->router(request);
        });

        try {
            listener
                .open()
                .then([&addr](){ std::cout << "Storage server is listening at: " << addr << std::endl; });
            while (1);
        } catch (const std::exception& e) {
            std::cout << "An error occurred: " << e.what() << std::endl;
        }
    }

    void router(http_request request) {
        auto p = ApiUtils::splitApiPath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string key = p.second;

        if (endpoint == U("/"))
        {
            if (request.method() == methods::GET)
                this->getHandler(request, key);
            if (request.method() == methods::PUT)
                this->putHandler(request, key);
            if (request.method() == methods::DEL)
                this->deleteHandler(request, key);
        }

        else 
        {
            std::cout << "Endpoint not implemented: " << endpoint << std::endl;
            return;
        }
    }
};

void run()
{
    StorageServer storageServer = StorageServer();
    storageServer.startServer();
}

int main()
{
    run();
}