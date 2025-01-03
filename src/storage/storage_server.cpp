#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <string>
#include <iostream>
#include <cstdlib>
#include <set>

#include "block.hpp"
#include "utils.hpp"
#include "disk_storage.hpp"
#include "storage_config.hpp"
#include "payloads.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class StorageServer
{
private:

    /**
     * On-disk storage for this node
     */
    std::unique_ptr<DiskStorage> diskStorage;

    StorageConfig config;

public:

    /**
     * Param constructor
     */
    StorageServer(std::string configFilePath)
        : config(configFilePath)
    {
        // initialise on-disk storage
        std::string storeDirPath = config.storeDirPath;
        std::string storeFileName = config.storeFilePrefix + std::to_string(getNodeIDFromEnv());
        uint32_t diskBlockSize = config.diskBlockSize;
        uint32_t maxDataSize = 1u << config.maxDataSizePower;
        bool removeExistingStoreFile = config.removeExistingStoreFile;
        uint32_t keyLengthMax = config.keyLengthMax;
        
        this->diskStorage = std::make_unique<DiskStorage>(
            storeDirPath,
            storeFileName,
            diskBlockSize,
            maxDataSize,
            removeExistingStoreFile,
            keyLengthMax
        );
    }

    /**
     * Retreives blocks of the given `key` from storage.
     */
    void getHandler(http_request request, std::string key)
    {
        std::cout << "GET /store req received: " << key << std::endl;

        std::vector<unsigned char> readBuffer;
        std::vector<Block> blocks;

        /**
         * Retreive block numbers from request payload
         */
        std::unordered_set<uint32_t> blockNums;

        auto task = request.extract_vector()
        .then([&](std::vector<unsigned char> payload)
        {
            auto it = payload.begin();
            while (it != payload.end())
            {
                uint32_t blockNum;
                std::memcpy(&blockNum, &(*it), sizeof(blockNum));
                it += sizeof(blockNum);

                blockNums.insert(blockNum);
            }
        });

        task.wait();

        /**
         * Retreive requested blocks from disk.
         * 
         * NOTE: 
         * 
         * Each block's data pointers point to positions in `readBuffer`.
         */
        try 
        {
            blocks = diskStorage->readBlocks(key, blockNums, config.dataBlockSize, readBuffer);
        }
        catch (std::runtime_error &e)
        {
            std::cout << e.what() << std::endl;
            request.reply(status_codes::InternalError);
            return;
        }
        
        // serialize blocks into response payload
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
     * Writes given blocks to storage for the given key `key`.
     */
    void putHandler(http_request request, std::string key)
    {
        std::cout << "PUT /store req received: " << key << std::endl;

        auto payloadBuffer = std::make_shared<std::vector<unsigned char>>();

        /**
         * Extract list of blocks from payload and write
         * them to disk.
         */
        pplx::task<void> task = request.extract_vector()
        .then([&](std::vector<unsigned char> payload)
        {
            *payloadBuffer = std::move(payload);
            std::vector<Block> blocks = Block::deserialize(*payloadBuffer);
            
            try
            {
                diskStorage->writeBlocks(key, blocks);
            }
            catch (std::runtime_error &e)
            {
                std::cout << e.what() << std::endl;
                request.reply(status_codes::InternalError);
                return;
            }

        });

        task.wait();

        std::vector<unsigned char> responseBuffer = createSizeResponsePayload();

        // send success response
        http_response response;
        response.set_status_code(status_codes::OK);
        response.set_body(responseBuffer);
        request.reply(response);
        return;
    }
    
    /**
     * Deletes all blocks of the given key `key` from this node.
     */
    void deleteHandler(http_request request, std::string key)
    {
        std::cout << "DEL /store req received: " << key << std::endl;
        try
        {
            diskStorage->deleteBlocks(key);
        }
        catch (std::runtime_error &e)
        {
            std::cout << e.what() << std::endl;
            request.reply(status_codes::InternalError);
        }

        std::vector<unsigned char> responseBuffer = createSizeResponsePayload();

        // send success response
        http_response response;
        response.set_status_code(status_codes::OK);
        response.set_body(responseBuffer);
        request.reply(response);
        return;
    }

    void syncHandler(http_request request)
    {
        std::cout << "GET /sync req received" << std::endl;
        std::vector<unsigned char> responseBuffer = createSyncResponsePayload();

        // send success response
        http_response response;
        response.set_status_code(status_codes::OK);
        response.set_body(responseBuffer);
        request.reply(response);
        return;
    }

    /**
     * The response payload of a /sync GET request is a 'sync response', 
     * which is of the form:
     * 
     *      ----
     *      key0
     *      numBlocks      
     *      blockNumA
     *      blockNumB
     *      ...
     *      ----
     *      key1
     *      numBlocks
     *      blockNumA
     *      blockNumB
     *      ...
     *      ----
     *      ...
     * 
     * NOTE:
     * 
     * 
     */
    std::vector<unsigned char> createSyncResponsePayload()
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;
        std::vector<std::string> keys = this->diskStorage->getKeys();
        for (std::string &key : keys)
        {
            std::vector<uint32_t> blockNums = this->diskStorage->getBlockNums(key, this->config.dataBlockSize);
            keyBlockNumMap[key] = blockNums;
        }

        Payloads::SizeInfo sizeInfo(this->diskStorage->dataUsedSize(), this->diskStorage->dataTotalSize());
        Payloads::SyncInfo syncInfo(keyBlockNumMap, sizeInfo);
        std::vector<unsigned char> buffer;
        syncInfo.serialize(buffer);

        return buffer;
    }

    /**
     * The response payload of a /store PUT/DEL request is a 'size response',
     * which is of the form:
     * 
     *      dataUsedSize - 4 bytes
     *      dataTotalSize - 4 bytes
     * 
     * where:
     *      dataUsedSize - num. bytes used of the data section
     *      dataTotalSize - total size of data section in bytes
     */
    std::vector<unsigned char> createSizeResponsePayload()
    {
        uint32_t dataUsedSize = this->diskStorage->dataUsedSize();
        uint32_t dataTotalSize = this->diskStorage->dataTotalSize();

        Payloads::SizeInfo sizeInfo(dataUsedSize, dataTotalSize);
        std::vector<unsigned char> buffer; 
        sizeInfo.serialize(buffer);

        return buffer;
    }

    /**
     * Responds to the master server's health check.
     */
    void healthCheckHandler(http_request request)
    {
        // std::cout << "GET /health req received" << std::endl;

        /**
         * If it can receive the request, it's healthy.
         * 
         * NOTE: In future, perhaps also check health of disk
         *       storage.
         */
        request.reply(status_codes::OK);
        return;
    }

    /**
     * Retreives node's unique id via the environment variable `NODE_ID`.
     */
    int getNodeIDFromEnv() 
    {
        const char* envNodeID = std::getenv("NODE_ID");
        return envNodeID ? std::stoi(envNodeID) : 0;
    }

    void startServer() 
    {
        uri_builder uri("http://0.0.0.0:8080");
        auto addr = uri.to_uri().to_string();
        http_listener listener(addr);

        listener.support([this](http_request request) {
            this->router(request);
        });

        try 
        {
            listener
                .open()
                .then([&addr](){ std::cout << "Storage server is listening at: " << addr << std::endl; });
            while (1);
        } 
        catch (const std::exception& e) 
        {
            std::cout << "An error occurred: " << e.what() << std::endl;
        }
    }

    void router(http_request request) 
    {
        auto p = ApiUtils::parsePath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string key = p.second;

        // network truncates the key's null bytes. Store as fixed size on disk.
        key = StringUtils::fixedSize(key, this->config.keyLengthMax);

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
        else if (endpoint == U("/sync"))
        {
            if (request.method() == methods::GET)
                this->syncHandler(request);
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
    std::string configFilePath = "/app/config.json";
    // std::string configFilePath = "../src/config.json";
    StorageServer storageServer = StorageServer(configFilePath);
    storageServer.startServer();
    // DiskStorageTests::runAll();
}

int main()
{
    run();
}