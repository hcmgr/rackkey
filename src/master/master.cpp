#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>
#include <map>
#include <chrono>

#include "hash_ring.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "block.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class MasterServer 
{
private:
    /**
     * Create and add our pre-defined storage nodes to the hash ring.
     */
    void addInitialStorageNodes() {
        for (std::string ip : this->config.storageNodeIPs) {
            hashRing.addNode(ip, this->config.numVirtualNodes);
        }
    }

public:
    /**
     * HashRing used to distribute our blocks evenly across nodes
     */
    HashRing hashRing;

    /**
     * Mapping of the form: KEY -> {blockNum -> storageNodeId}.
     * 
     * i.e. for each KEY, we store a mapping from block number to storage node
     * 
     * Allows for fast lookup of block location.
     * 
     * NOTE: nicknamed the 'KBN' for brevity
     */
    std::map<std::string, std::shared_ptr<std::map<int, int>> > keyBlockNodeMap;

    /**
     * Current open connections 
     */
    std::map<int, std::shared_ptr<http_client>> openConnections;

    /**
     * Stores configuration of our service (e.g. storage node IPs)
     */
    Config config;

    /* Default constructor */
    MasterServer(std::string configFilePath) 
        : hashRing(),
          keyBlockNodeMap(),
          config(configFilePath)
    {
        addInitialStorageNodes();
    }

    ~MasterServer() 
    {

    }

    /**
     * Calculates and displays the blocks distribution
     * across the storage nodes.
     */
    void calculateAndShowBlockDistribution() 
    {
        std::map<int, int> nodeBlockCounts;

        for (auto p : this->keyBlockNodeMap) 
        {
            std::string key = p.first;
            std::shared_ptr<std::map<int, int>> blockNodeMap = p.second;

            for (auto p : *blockNodeMap) 
            {
                if (nodeBlockCounts.find(p.second) == nodeBlockCounts.end())
                    nodeBlockCounts[p.second] = 0;

                nodeBlockCounts[p.second]++;
            }
        }

        PrintUtils::printMap(nodeBlockCounts);
    }

    /**
     * GET
     * ---
     * Given: (KEY)
     * Requests all blocks for the given KEY from the storage cluster, returns in order
     */
    void getHandler(http_request request, const std::string key) 
    {
        std::cout << "GET req received: " << key << std::endl;

        // Create HTTP client
        http_client client(U("http://localhost:8081"));

        // Send a GET request
        client.request(methods::GET, U("/"))
            .then([&](http_response response) {
                if (response.status_code() == status_codes::OK) {
                    // Parse the response body as JSON
                    json::value jsonResponse = response.extract_json().get();
                    std::cout << jsonResponse << std::endl;
                } else {
                    std::cout << "Request failed with status: " << response.status_code() << std::endl;
                }
            })
            .wait();

        return;
    }

    /**
     * Sends the given list of blocks to the given physical node.
     * 
     * All blocks are sent in a single PUT request to the given 
     * physical node.
     */
    pplx::task<void> sendBlocks(
        int physicalNodeId,
        std::vector<Block> &blocks,
        std::shared_ptr<std::map<int, int>> blockNodeMap
    )
    {
        std::shared_ptr<PhysicalNode> pn = this->hashRing.getPhysicalNode(physicalNodeId);

        // initialise / retreive client
        if (this->openConnections.find(pn->id) == this->openConnections.end())
            this->openConnections[pn->id] = std::make_shared<http_client>(U(pn->ip));
        http_client& client = *this->openConnections[pn->id];

        // populate body
        std::vector<unsigned char> payloadBuffer;
        for (auto block : blocks)
        {
            block.serialize(payloadBuffer);
        }

        // build request
        http_request req = http_request();
        req.set_method(methods::PUT);
        req.set_request_uri(U("/"));
        req.set_body(payloadBuffer);

        pplx::task<void> task = client.request(req)
            // send request
            .then([=](http_response response) 
            {
                if (response.status_code() == status_codes::OK) {
                    json::value jsonResponse = response.extract_json().get();
                } else {
                    std::cout << "Request failed with status: " << response.status_code() << std::endl;
                }
            })

            // assign blocks to their nodes
            .then([=]()
            {
                for (auto block : blocks)
                {
                    blockNodeMap->insert({block.blockNum, physicalNodeId});
                }
            });

        return task;
    }

    /**
     * PUT
     * ---
     * Given: (KEY, payload)
     * Breaks payload into blocks and distributes them across the storage cluster.
     */
    void putHandler(http_request request, const std::string key) 
    {
        // Timing point: start
        auto start = std::chrono::high_resolution_clock::now();
            
        std::shared_ptr<std::map<int, int>> blockNodeMap = std::make_shared<std::map<int, int>>();
        std::vector<pplx::task<void>> blockSendTasks;

        std::shared_ptr<std::vector<unsigned char>> payloadPtr = std::make_shared<std::vector<unsigned char>>();

        pplx::task<void> task = request.extract_vector()

        // divide into blocks
        .then([&](std::vector<unsigned char> payload)
        {
            auto blockStart = std::chrono::high_resolution_clock::now();
            std::cout << "TIME - extract: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(blockStart - start).count() 
                  << " ms"
                  << std::endl;
            
            *payloadPtr = std::move(payload);
                  
            int payloadSize = payloadPtr->size();
            int blockCnt = 0;

            std::map<int, std::vector<Block>> nodeBlockMap;

            for (int i = 0; i < payloadSize; i += config.blockSize) 
            {
                int blockNum = blockCnt++;

                std::string hashInput = key + std::to_string(blockNum);
                uint32_t hash = Crypto::sha256_32(hashInput);
                std::shared_ptr<VirtualNode> vn = this->hashRing.findNextNode(hash);
                
                auto blockStart = payloadPtr->begin() + i;
                auto blockEnd = payloadPtr->begin() + std::min(i + config.blockSize, payloadSize);
                auto blockSize = blockEnd - blockStart;

                Block block(key, blockNum, blockSize, blockStart, blockEnd);
                nodeBlockMap[vn->physicalNodeId].push_back(std::move(block));
            }

            std::cout << "Total blocks: " << blockCnt << std::endl;

            auto blockEnd = std::chrono::high_resolution_clock::now();
            std::cout << "TIME - block: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(blockEnd - blockStart).count() 
                  << " ms"
                  << std::endl;

            return nodeBlockMap;
        })

        // send all blocks for given node at once
        .then([&](std::map<int, std::vector<Block>> nodeBlockMap)
        {
            auto sendStart = std::chrono::high_resolution_clock::now();

            for (auto p : nodeBlockMap)
            {
                int physicalNodeId = p.first;
                std::vector<Block> blocks = p.second;

                auto task = sendBlocks(physicalNodeId, blocks, blockNodeMap);
                blockSendTasks.push_back(task);
            }

            // wait for 'send' tasks
            return pplx::when_all(blockSendTasks.begin(), blockSendTasks.end())
            .then([=]()
            {
                auto sendEnd = std::chrono::high_resolution_clock::now();
                std::cout << "TIME - send & receive: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(sendEnd - sendStart).count() 
                  << " ms"
                  << std::endl;

                this->keyBlockNodeMap[key] = blockNodeMap;
            });
        });
        
        task.wait();

        calculateAndShowBlockDistribution();

        // Timing point: end
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Total Time: " << duration.count() << " ms" << std::endl;

        // send response
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
    void deleteHandler(http_request request, const std::string key) 
    {
    }

    void router(http_request request) {
        auto p = ApiUtils::splitApiPath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string key = p.second;

        std::cout << key << std::endl;

        if (endpoint == U("/store"))
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

    void startServer() 
    {
        uri_builder uri(this->config.masterServerIPPort);
        auto addr = uri.to_uri().to_string();
        http_listener listener(addr);

        listener.support([this](http_request request) {
            this->router(request);
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
};

////////////////////////////////////////////
// MasterServer tests
////////////////////////////////////////////

namespace MasterServerTests {
};


////////////////////////////////////////////
// Run
////////////////////////////////////////////

void run()
{    
    std::string configFilePath = "../config.json";
    MasterServer masterServer = MasterServer(configFilePath);
    masterServer.startServer();
}

int main() 
{
    run();
}