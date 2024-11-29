#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>
#include <map>
#include <chrono>

#include "hash_ring.hpp"
#include "utils.hpp"
#include "config.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

/**
 * Represents an I/O block.
 * 
 * NOTE: Block objects only help organise I/O and are NOT kept 
 *       around in memory. i.e. they last the duration of the
 *       GET/PUT request.
 */
class Block {
public:
    int blockNum;
    int size;
    std::vector<unsigned char>::iterator start;
    std::vector<unsigned char>::iterator end;

    Block(
        int blockNum,
        int size,
        std::vector<unsigned char>::iterator start,
        std::vector<unsigned char>::iterator end
    ) 
        : blockNum(blockNum),
          size(size),
          start(start),
          end(end)
    {
    }

    void prettyPrintBlock() 
    {
        std::cout << "Block: " << blockNum << std::endl;
        std::cout << "size: " << size << " bytes" << std::endl;
    }
};

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
    void calculateAndShowBlockDistribution() {
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
    void getHandler(http_request request) 
    {
        std::cout << "get req received" << std::endl;

        // Create HTTP client
        http_client client(U("http://192.168.0.24:8082"));

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
     * Send block to designated storage node
     */
    pplx::task<void> sendBlocks(
        int physicalNodeId,
        std::vector<Block> &blocks,
        std::shared_ptr<std::map<int, int>> blockNodeMap
    )
    {
        std::cout << "Sending blocks to node: " << physicalNodeId << std::endl;

        std::shared_ptr<PhysicalNode> pn = this->hashRing.getPhysicalNode(physicalNodeId);

        if (this->openConnections.find(pn->id) == this->openConnections.end())
            this->openConnections[pn->id] = std::make_shared<http_client>(U(pn->ip));
        http_client& client = *this->openConnections[pn->id];

        // TODO: fill request with data

        pplx::task<void> task = client.request(methods::GET, U("/"))
            // send request
            .then([=](http_response response) 
            {
                if (response.status_code() == status_codes::OK) {
                    std::cout << "resp received from node: " << physicalNodeId << std::endl;
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
     * Breaks payload into blocks and distributes them across the storage cluster
     */
    void putHandler(http_request request) 
    {
        std::cout << "put req received" << std::endl;

        // TODO: extract {KEY} from request
        std::string key = "archive.zip";

        // Timing point: start
        auto start = std::chrono::high_resolution_clock::now();
            
        std::shared_ptr<std::map<int, int>> blockNodeMap = std::make_shared<std::map<int, int>>();
        std::vector<pplx::task<void>> blockSendTasks;

        pplx::task<void> task = request.extract_vector()

        // divide into blocks
        .then([&](std::vector<unsigned char> payload)
        {
            auto blockStart = std::chrono::high_resolution_clock::now();
            std::cout << "TIME - extract: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(blockStart - start).count() 
                  << std::endl;
                  
            int payloadSize = payload.size();
            int blockCnt = 0;

            std::map<int, std::vector<Block>> nodeBlockMap;

            for (int i = 0; i < payloadSize; i += config.blockSize) 
            {
                int blockNum = blockCnt++;

                std::string hashInput = key + std::to_string(blockNum);
                uint32_t hash = Crypto::sha256_32(hashInput);
                std::shared_ptr<VirtualNode> vn = this->hashRing.findNextNode(hash);
                
                auto blockStart = payload.begin() + i;
                auto blockEnd = payload.begin() + std::min(i + config.blockSize, payloadSize);
                auto blockSize = blockEnd - blockStart;

                Block block(blockNum, blockSize, blockStart, blockEnd);
                nodeBlockMap[vn->physicalNodeId].push_back(std::move(block));
            }

            auto blockEnd = std::chrono::high_resolution_clock::now();
            std::cout << "TIME - block: " 
                  << std::chrono::duration_cast<std::chrono::milliseconds>(blockEnd - blockStart).count() 
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
    void deleteHandler(http_request request) 
    {
    }

    void startServer() {
        uri_builder uri(this->config.masterServerIPPort);
        auto addr = uri.to_uri().to_string();
        http_listener listener(addr);

        listener.support(methods::GET, [this](http_request request) {
            this->getHandler(request);
        });
        listener.support(methods::PUT, [this](http_request request) {
            this->putHandler(request);
        });
        listener.support(methods::DEL, [this](http_request request) {
            this->deleteHandler(request);
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

    void router() {

    }
};

////////////////////////////////////////////
// MasterServer tests
////////////////////////////////////////////

namespace MasterServerTests {
    void testCanAddInitialStorageNodes() {
        std::string configFilePath = "../config.json";
        MasterServer masterServer = MasterServer(configFilePath);
        masterServer.hashRing.prettyPrintHashRing();
    }
};

int main() 
{
    std::string configFilePath = "../config.json";
    MasterServer masterServer = MasterServer(configFilePath);
    masterServer.startServer();
}