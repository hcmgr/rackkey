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
    std::vector<unsigned char> data;

    Block(int blockNum, int size, std::vector<unsigned char> data) 
        : blockNum(blockNum),
          size(size),
          data(data)
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

        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "1" << std::endl;
        
        pplx::task<void> task = request.extract_vector()

        // break up payload into blocks (i.e. build up 'blockList')
        .then([&](std::vector<unsigned char> payload)
        {
            std::cout << "2" << std::endl;
            std::shared_ptr<std::vector<Block>> blockList = std::make_shared<std::vector<Block>>();

            int payloadSize = payload.size();
            int blockCnt = 0;

            for (int i = 0; i < payloadSize; i += config.blockSize) {
                size_t blockEnd = std::min(i + config.blockSize, payloadSize);
                std::vector<unsigned char> blockData(payload.begin() + i, payload.begin() + blockEnd);
                blockList->emplace_back(blockCnt++, blockData.size(), std::move(blockData));
            }

            std::cout << "3" << std::endl;
            return blockList;
        })

        // send each block, building up block->node map in the process
        .then([&](std::shared_ptr<std::vector<Block>> blockList) 
        {
            std::shared_ptr<std::map<int, int>> blockNodeMap = std::make_shared<std::map<int, int>>();
            for (Block &block : *blockList) 
            {
                // find storage node
                std::string hashInput = key + std::to_string(block.blockNum);
                uint32_t hash = Crypto::sha256_32(hashInput);
                std::shared_ptr<VirtualNode> vn = this->hashRing.findNextNode(hash);
                int physicalNodeId = vn->physicalNodeId;

                // TODO: write block to physical node, only continue on success

                // add block->node entry
                blockNodeMap->insert({block.blockNum, physicalNodeId});
            }
            std::cout << "4" << std::endl;

            return blockNodeMap;
        })

        // add block->node mapping to KBN
        .then([&](std::shared_ptr<std::map<int, int>> blockNodeMap) 
        {
            this->keyBlockNodeMap[key] = blockNodeMap;
            std::cout << "5" << std::endl;
        });

        task.wait();

        calculateAndShowBlockDistribution();
        std::cout << "6" << std::endl;

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        std::cout << "Time: " << duration.count() << " ms" << std::endl;

        // send resposne
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