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
#include "test_utils.hpp"

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
     * Calculates and displays the distribution of the given `key`s blocks
     * across the storage nodes.
     */
    void calculateAndShowBlockDistribution(std::string key) 
    {
        std::map<int, int> nodeBlockCounts; // node id -> block count
        int totalNumBlocks = this->keyBlockNodeMap[key]->size();
        
        for (auto p : *(this->keyBlockNodeMap[key]))
        {
            int nodeId = p.second;

            if (nodeBlockCounts.find(nodeId) == nodeBlockCounts.end())
                nodeBlockCounts[nodeId] = 0;

            nodeBlockCounts[nodeId]++;
        }

        std::cout << "Blocks: " << totalNumBlocks << std::endl;
        std::cout << "Distribution:" << std::endl;
        PrintUtils::printMap(nodeBlockCounts);
    }

    /**
     * Retreives all blocks from given physical node
     */
    pplx::task<void> getBlocks(
        int physicalNodeId,
        std::string key,
        std::vector<int> blockNums,
        std::shared_ptr<std::map<int, Block>> blockMap
    )
    {
        std::shared_ptr<PhysicalNode> pn = this->hashRing.getPhysicalNode(physicalNodeId);

        // initialise / retreive client
        if (this->openConnections.find(pn->id) == this->openConnections.end())
            this->openConnections[pn->id] = std::make_shared<http_client>(U(pn->ip));
        http_client& client = *this->openConnections[pn->id];

        // build request
        http_request req = http_request();
        req.set_method(methods::GET);
        req.set_request_uri(U("/" + key));
        // TODO: set body to blockNums encoded as unsigned chars

        auto payloadPtr = std::make_shared<std::vector<unsigned char>>();

        return client.request(req)
        .then([=](http_response response)
        {
            return response.extract_vector();
        })
        .then([=](std::vector<unsigned char> payload)
        {
            *payloadPtr = payload;
            std::vector<Block> blocks = Block::deserialize(*(payloadPtr));
            for (auto block : blocks)
            {
                blockMap->insert({block.blockNum, std::move(block)});
            }
        });
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

        // check key exists
        if (this->keyBlockNodeMap.find(key) == this->keyBlockNodeMap.end())
        {
            std::cout << "GET: failed - key doesn't exist" << std::endl;
            http_response response(status_codes::BadRequest);
            request.reply(response);
            return;
        }

        std::shared_ptr<std::map<int, int>> blockNodeMap = this->keyBlockNodeMap[key];
        
        /**
         * Build up a mapping of the form: {node id -> block numbers}
         */
        std::map<int, std::vector<int>> nodeBlockMap;
        for (auto p : *(blockNodeMap))        
        {
            int blockNum = p.first;
            int nodeId = p.second;
            nodeBlockMap[nodeId].push_back(blockNum);
        }

        // mapping of the form: {block num. -> block object}
        // NOTE: each call to `getBlocks` builds this up
        auto blockMap = std::make_shared<std::map<int, Block>>();
        
        // call 'getBlocks' for each node, sending block nums as payload
        std::vector<pplx::task<void>> getBlockTasks;
        for (auto p : nodeBlockMap)
        {
            int nodeId = p.first;
            std::vector<int> blockNums = p.second;

            auto task = getBlocks(nodeId, key, blockNums, blockMap);
            getBlockTasks.push_back(task);
        }

        auto task = pplx::when_all(getBlockTasks.begin(), getBlockTasks.end());
        task.wait();

        // recombine
        std::vector<unsigned char> payloadBuffer;
        for (auto p : *(blockMap))
        {
            Block block = p.second;
            payloadBuffer.insert(payloadBuffer.end(), block.dataStart, block.dataEnd);
        }

        std::cout << "GET: successful" << std::endl;

        // send response
        http_response response(status_codes::OK);
        response.set_body(payloadBuffer);
        request.reply(response);
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
        std::string key,
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
        req.set_request_uri(U("/" + key));
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
        std::cout << "PUT req received: " << key << std::endl;

        // Timing point: start
        auto start = std::chrono::high_resolution_clock::now();
            
        auto blockNodeMap = std::make_shared<std::map<int, int>>();
        auto payloadPtr = std::make_shared<std::vector<unsigned char>>();

        std::vector<pplx::task<void>> sendBlockTasks;

        pplx::task<void> task = request.extract_vector()

        // divide into blocks
        .then([&](std::vector<unsigned char> payload)
        {
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

                auto task = sendBlocks(physicalNodeId, key, blocks, blockNodeMap);
                sendBlockTasks.push_back(task);
            }

            // wait for 'send' tasks
            return pplx::when_all(sendBlockTasks.begin(), sendBlockTasks.end())
            .then([=]()
            {
                this->keyBlockNodeMap[key] = blockNodeMap;
            });
        });
        
        task.wait();

        calculateAndShowBlockDistribution(key);

        // Timing point: end
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Total Time: " << duration.count() << " ms" << std::endl;

        std::cout << "PUT: successful" << std::endl;

        // send response - empty body for now
        http_response response(status_codes::OK);
        request.reply(response);
        return;
    }

    /**
     * DELETE
     * ---
     * Given: (KEY)
     * Deletes all blocks of the given KEY from the storage cluster
     */
    void deleteHandler(http_request request, const std::string key) 
    {
        std::cout << "DEL req received: " << key << std::endl;

        std::vector<pplx::task<void>> delBlockTasks;

        /**
         * - Find out nodes that have `key`s blocks
         * - ask them to remove all blocks for `key`
         */
    }

    /**
     * GET
     * ---
     * Returns newline-separated list of all keys.
     */
    void getKeysHandler(http_request request) 
    {
        std::cout << "GET /keys req received" << std::endl;

        std::ostringstream oss;
        for (const auto &p : keyBlockNodeMap)
            oss << p.first << "\n";
        
        request.reply(status_codes::OK, oss.str());
    }

    void router(http_request request) {
        auto p = parsePath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string param = p.second;

        if (endpoint == U("/store"))
        {
            if (request.method() == methods::GET)
                this->getHandler(request, param);
            if (request.method() == methods::PUT)
                this->putHandler(request, param);
            if (request.method() == methods::DEL)
                this->deleteHandler(request, param);
        }

        else if (endpoint == U("/keys") && param == U(""))
        {
            if (request.method() == methods::GET)
                this->getKeysHandler(request);
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
                .then([&addr](){ std::cout << "Master server is listening at: " << addr << std::endl; });
            while (1);
        } catch (const std::exception& e) {
            std::cout << "An error occurred: " << e.what() << std::endl;
        }
    }

    /**
     * Utility function to parse the given uri into a {path, param} pair.
     * 
     * E.g.
     * 
     * "/store/key1" OR "/store/key1/" -> {"/store", "key1"}
     * 
     * E.g.
     * 
     * "/keys" OR "/keys/" -> {"/keys", ""}
     */
    std::pair<std::string, std::string> parsePath(const std::string &uri) 
    {
        std::string cleanUri = uri;

        // remove ending '/', if present
        if (cleanUri[cleanUri.size() - 1] == '/')
            cleanUri = cleanUri.substr(0, cleanUri.size() - 1);

        size_t lastSlashPos = cleanUri.find_last_of('/');

        if (lastSlashPos == std::string::npos)
            return {cleanUri, ""};
        
        if (lastSlashPos == 0)
            return {cleanUri, ""};

        std::string prefix = cleanUri.substr(0, lastSlashPos);
        std::string key = cleanUri.substr(lastSlashPos + 1);

        return {prefix.empty() ? cleanUri : prefix, key};
    } 
};

////////////////////////////////////////////
// MasterServer tests
////////////////////////////////////////////

namespace MasterServerTests {
    void testParsePath()
    {
        MasterServer ms("../config.json");

        std::vector<std::string> paths = {
            "/store/archive.zip",
            "/store/archive.zip/",

            "/keys",
            "/keys/"
        };

        std::vector<std::pair<std::string, std::string>> expectedParsedPaths = {
            {"/store", "archive.zip"},
            {"/store", "archive.zip"},

            {"/keys", ""},
            {"/keys", ""}
        };

        for (int i = 0; i < paths.size(); i++)
        {
            auto p = ms.parsePath(paths[i]);
            std::cout << p.first << " " << p.second << std::endl;
            // ASSERT_THAT(p == expectedParsedPaths[i]);
        }
    }
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

/*
TODO:
    - implement DEL (both master AND server)
    - implement GET keys
    - group together and separate out handlers into nice abstraction
    - figure out why block allocations are not consistent
      across tests of the same files
    - understand and internalise CAP
    - replication
        - assume all nodes healthy for now

    - periodic health checking
    - implement concurrent r/w protections for DiskStorage
        - see bottom of server.cpp for plan
    - sort out documentation

WITH REPLICATION PLAN:
    GET:
        - KBN: { key -> {blockNum -> nodeIdList} }
            - nodeIdList.size() == R, where R is our replication factor
        - for each block, greedily pick first 'healthy' node (as per latest health check)
        - build up {nodeId -> blockNumList}
            - so can query node's blocks in one request
        - send storage a GET, with blockNumList as payload
    
    WRITE:
        - for each block:
            - hash(key + blockNum) -> R nodes to store on
            - i.e. KBN[key][blockNum] = {node0, node1, ... nodeR}
        - {blockNum -> nodeList} map to build up {nodeId -> blockNumList} map
        - for each nodeId:
            - build up vector<Block> blocks
            - call sendBlocks(nodeId, key, blocks)
*/