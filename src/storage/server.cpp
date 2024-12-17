#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <string>
#include <iostream>
#include <cstdlib>

#include "block.hpp"
#include "utils.hpp"
#include "disk_storage.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class StorageServer
{
private:
    /**
     * On-disk storage
     */
    DiskStorage diskStorage;

public:

    /**
     * Default constructor
     */
    StorageServer()
        : diskStorage("rackkey", "store", 4096, 1u << 30, true)
    {
    }

    /**
     * Retreives blocks of the given `key` from storage.
     * 
     * TODO: fix hard coding of block size
     */
    void getHandler(http_request request, std::string key)
    {
        std::cout << "GET req received: " << key << std::endl;

        std::vector<unsigned char> readBuffer;
        std::vector<Block> blocks;
        uint32_t dataBlockSize = 4096;

        try 
        {
            blocks = diskStorage.readBlocks(key, dataBlockSize, readBuffer);
        }
        catch (std::runtime_error &e)
        {
            std::cout << e.what() << std::endl;
            request.reply(status_codes::InternalError);
            return;
        }
        
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

        auto payloadPtr = std::make_shared<std::vector<unsigned char>>();

        pplx::task<void> task = request.extract_vector()

        // extract list of blocks
        .then([&](std::vector<unsigned char> payload)
        {
            *payloadPtr = std::move(payload);

            std::vector<Block> blocks = Block::deserialize(*payloadPtr);
            
            try
            {
                diskStorage.writeBlocks(key, blocks);
            }
            catch (std::runtime_error &e)
            {
                std::cout << e.what() << std::endl;
                request.reply(status_codes::InternalError);
                return;
            }

        });

        task.wait();

        request.reply(status_codes::OK);
        return;
    }
    
    /**
     * Deletes all blocks of the given `key` from this node.
     */
    void deleteHandler(http_request request, std::string key)
    {
        std::cout << "DEL req received: " << key << std::endl;
        try
        {
            diskStorage.deleteBlocks(key);
        }
        catch (std::runtime_error &e)
        {
            std::cout << e.what() << std::endl;
            request.reply(status_codes::InternalError);
        }

        request.reply(status_codes::OK);
        return;
    }

    /**
     * Responds to the master server's health check.
     */
    void healthCheckHandler(http_request request)
    {
        std::cout << "/health/ req received" << std::endl;

        /**
         * If it can receive the request, it's healthy.
         * 
         * NOTE: In future, perhaps also check health of disk
         *       storage.
         */
        request.reply(status_codes::OK);
        return;
    }

    void startServer() 
    {
        uri_builder uri("http://0.0.0.0:8080");
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
        auto p = ApiUtils::parsePath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string key = p.second;

        if (endpoint == U("/store"))
        {
            if (request.method() == methods::GET)
                this->getHandler(request, key);
            if (request.method() == methods::PUT)
                this->putHandler(request, key);
            if (request.method() == methods::DEL)
                this->deleteHandler(request, key);
        }
        else if (endpoint == U("/health"))
        {
            if (request.method() == methods::GET)
                this->healthCheckHandler(request);
        }
        else 
        {
            std::cout << "Endpoint not implemented: " << endpoint << std::endl;
            request.reply(status_codes::NotImplemented);
            return;
        }
    }
};

void run()
{
    StorageServer storageServer = StorageServer();
    storageServer.startServer();

    // DiskStorageTests::runAll();
}

int main()
{
    run();
}

/*
Immediate todo:
    - checks in server.cpp that:
        - blocks are in correct order (disk_storage presumes they are)
        - first (blockNum - 1) blocks are full
    - handle hash collisions of hash(key)
        - heh?
    - handle concurrent r/w of storeFile (below)

Maybe:
    - handle concurrent r/w
        - i.e. use locks to prevent multiple threads
          r/w store file at same time
        - master may have multiple threads call same node
          concurrently, in which case multiple storage server
          threads will access same DiskStorage simultaneously
        - can probably just lock the entire DiskStorage for now
        - better approach:
            - when read, obtain a shared lock 
                - wait for any exclusive lock
            - when write, obtain an exclusive lock
                - wait for any exclusive AND shared lock
    
General cleanup:
    - make helper method for opening and closing store file
        - looks the same each time we do it
    - nice explanation at top of disk_storage.cpp/.hpp
    - emphasise difference between diskBlocks and dataBlocks
        - ehhh...we are now only storing the data in our 
          on-disk blocks, so there really is no distinction
        - only potential complexity is that:
            - for us, data block size == disk block size (always)
            - think through consequences of allowing data vs disk block
              size to be difference
    - storage vs store naming? (DiskStorage -> StoreFile)?

Long term:
    - put servers on docker (once know stable)
    - master periodically 'health checks' servers
    - replication
*/