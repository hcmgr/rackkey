#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <map>
#include <set>
#include <chrono>
#include <thread>
#include <mutex>

#include "storage_node.hpp"
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
     * Add our pre-defined storage nodes and add their virtual nodes
     * to the hash ring.
     */
    void addInitialStorageNodes() {
        for (std::string ipPort : this->config.storageNodeIPs) 
        {
            auto storageNode = std::make_shared<StorageNode>(ipPort, this->config.numVirtualNodes);
            this->storageNodes[storageNode->id] = storageNode;

            // add all virtual nodes to the hash ring
            for (auto &vn : storageNode->virtualNodes)
                hashRing.addNode(vn);
            
            // node must prove beyond reasonable doubt its healthy
            this->nodeHealthMap[storageNode->id] = false;
        }
    }

public:
    /**
     * HashRing used to distribute our blocks evenly across nodes
     */
    HashRing hashRing;

    /**
     * Stores mapping of the form: key -> {block num -> storage node id}.
     * 
     * i.e. for each KEY, we store a mapping from block number to storage node
     * 
     * Allows for fast lookup of block location.
     * 
     * NOTE: nicknamed the 'KBN' for brevity
     */
    std::map<std::string, std::shared_ptr<std::map<int, int>>> keyBlockNodeMap;

    /**
     * Stores our storage nodes.
     * 
     * Mapping is of the form: storage node id -> StorageNode object.
     */
    std::map<int, std::shared_ptr<StorageNode>> storageNodes;

    /**
     * Stores currently open connections to storage nodes.
     * 
     * Mapping is of the form: storage node id -> http_client object.
     */
    std::map<int, std::shared_ptr<http_client>> openConnections;
    std::mutex openConnectionsMutex;

    /**
     * Stores 'health status' of nodes, as per last health check.
     * 
     * Mapping is of the form: storage node id -> 'health status' boolean
     */
    std::map<int, bool> nodeHealthMap;
    std::mutex nodeHealthMapMutex;

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
     * 
     * TODO: 
     * 
     * re-factor for replication
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
     * Thread function used to periodically check health of all storage nodes.
     * 
     * `period` - time to wait in between checks (milliseconds)
     */
    void checkNodeHealth()
    {
        while (1) 
        {
            /**
             * Every `period` milliseconds, send a GET to the /health/ endpoint
             * of each storage node.
             * 
             * Update our nodeHealthMap accordingly.
             */

            std::this_thread::sleep_for(
                std::chrono::milliseconds(this->config.healthCheckPeriodMs)
            );

            std::vector<pplx::task<void>> healthCheckTasks;
            for (auto p : storageNodes)
            {
                int nodeId = p.first;
                std::shared_ptr<StorageNode> sn = p.second;

                // initialise / retreive client
                if (this->openConnections.find(nodeId) == this->openConnections.end())
                    this->openConnections[nodeId] = std::make_shared<http_client>(U(sn->ipPort));
                http_client& client = *this->openConnections[nodeId];

                // build and send request
                http_request req = http_request();
                req.set_method(methods::GET);
                req.set_request_uri(U("/health/"));

                auto task = client.request(req)
                .then([this, nodeId](pplx::task<http_response> prevTask){
                    try
                    {
                        // can throw an http_exception if connection isn't established
                        http_response resp = prevTask.get();

                        bool isHealthy = (resp.status_code() == status_codes::OK);

                        {
                            std::lock_guard<std::mutex> lock(this->nodeHealthMapMutex);
                            this->nodeHealthMap[nodeId] = isHealthy;
                        }
                    }
                    catch (const http_exception &e)
                    {
                        // std::cout << "Node " << nodeId << ": " << e.what() << std::endl;
                        {
                            std::lock_guard<std::mutex> lock(this->nodeHealthMapMutex);
                            this->nodeHealthMap[nodeId] = false;
                        }
                    }
                });

                healthCheckTasks.push_back(task);
            }

            // wait on all request tasks - we're on a separate thread so this won't hurt
            auto task = pplx::when_all(healthCheckTasks.begin(), healthCheckTasks.end());
            task.wait();

            std::cout << std::endl << "Storage node health: " << std::endl;
            PrintUtils::printMap(this->nodeHealthMap);
        }
    }

    /**
     * Retreives all blocks from given storage node
     */
    pplx::task<void> getBlocks(
        int storageNodeId,
        std::string key,
        std::vector<int> blockNums,
        std::shared_ptr<std::map<int, Block>> blockMap
    )
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        // initialise / retreive client
        if (this->openConnections.find(sn->id) == this->openConnections.end())
            this->openConnections[sn->id] = std::make_shared<http_client>(U(sn->ipPort));
        http_client& client = *this->openConnections[sn->id];

        // build request
        http_request req = http_request();
        req.set_method(methods::GET);
        req.set_request_uri(U("/store/" + key));

        auto payloadPtr = std::make_shared<std::vector<unsigned char>>();

        return client.request(req)
        .then([=](http_response response)
        {
            if (response.status_code() != status_codes::OK)
            {
                throw std::runtime_error(
                    "getBlocks() failed with status: " + std::to_string(response.status_code()));
            }
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
     * 
     * NOTE: TODO:
     * 
     * At the moment, we don't send block numbers to get.
     * Will likely needs this in the future.
     */
    void getHandler(http_request request, const std::string key) 
    {
        std::cout << "GET req received: " << key << std::endl;

        // check key exists
        if (this->keyBlockNodeMap.find(key) == this->keyBlockNodeMap.end())
        {
            std::cout << "GET: failed - key doesn't exist" << std::endl;
            request.reply(status_codes::InternalError);
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
        
        /**
         * Call `getBlocks` for each node and wait on all tasks 
         * to finish.
         */
        std::vector<pplx::task<void>> getBlockTasks;
        for (auto p : nodeBlockMap)
        {
            int nodeId = p.first;
            std::vector<int> blockNums = p.second;

            auto task = getBlocks(nodeId, key, blockNums, blockMap);
            getBlockTasks.push_back(task);
        }

        bool success = true;
        auto task = pplx::when_all(getBlockTasks.begin(), getBlockTasks.end())
        .then([&request, &success](pplx::task<void> allTasks)
        {
            try 
            {
                allTasks.get();
            }
            catch (const std::exception& e)
            {
                std::cout << "GET: failed - " << e.what() << std::endl;
                success = false;

                request.reply(status_codes::InternalError);
            }
        });

        task.wait();

        if (!success)
            return;

        // recombine
        std::vector<unsigned char> payloadBuffer;
        for (auto p : *(blockMap))
        {
            Block block = p.second;
            payloadBuffer.insert(payloadBuffer.end(), block.dataStart, block.dataEnd);
        }

        std::cout << "GET: successful" << std::endl;

        // success response
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
        int storageNodeId,
        std::string key,
        std::vector<Block> &blocks,
        std::shared_ptr<std::map<int, int>> blockNodeMap
    )
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        // initialise / retreive client
        if (this->openConnections.find(sn->id) == this->openConnections.end())
            this->openConnections[sn->id] = std::make_shared<http_client>(U(sn->ipPort));
        http_client& client = *this->openConnections[sn->id];
        
        // populate body
        std::vector<unsigned char> payloadBuffer;
        for (auto block : blocks)
        {
            block.serialize(payloadBuffer);
        }

        // build request
        http_request req = http_request();
        req.set_method(methods::PUT);
        req.set_request_uri(U("/store/" + key));
        req.set_body(payloadBuffer);

        pplx::task<void> task = client.request(req)
            // send request
            .then([=](http_response response) 
            {
                if (response.status_code() != status_codes::OK) 
                {
                    throw std::runtime_error(
                        "sendBlocks() failed with status: " + std::to_string(response.status_code()));
                }
            })

            // assign blocks to their nodes
            .then([=]()
            {
                for (auto block : blocks)
                {
                    blockNodeMap->insert({block.blockNum, storageNodeId});
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
        bool success = true;

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
            .then([&](pplx::task<void> allTasks)
            {
                try
                {
                    allTasks.get();
                    this->keyBlockNodeMap[key] = blockNodeMap;
                }
                catch (const std::exception& e)
                {
                    std::cout << "PUT: failed - " << e.what() << std::endl;
                    success = false;
                    request.reply(status_codes::InternalError);
                }
            });
        });
        
        task.wait();

        if (!success)
            return;

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
     * Deletes all blocks correpsonding to key `key` from node with id `physicalNodeId`.
     */
    pplx::task<void> deleteBlocks(int storageNodeId, std::string key)
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        // initialise / retreive client
        if (this->openConnections.find(sn->id) == this->openConnections.end())
            this->openConnections[sn->id] = std::make_shared<http_client>(U(sn->ipPort));
        http_client& client = *this->openConnections[sn->id];

        // build and send request
        http_request req = http_request();
        req.set_method(methods::DEL);
        req.set_request_uri(U("/store/" + key));

        auto task = client.request(req)
        .then([=](http_response response)
        {
            if (response.status_code() != status_codes::OK)
            {
                throw std::runtime_error(
                    "deleteBlocks() failed with status: " + std::to_string(response.status_code()));
            }
        });

        return task;
    }

    /**
     * DELETE
     * ---
     * Given: (KEY)
     * Deletes all blocks of the given KEY from the storage cluster
     * 
     * NOTE: TODO:
     * 
     * At the moment, we don't send block numbers to delete.
     * Will likely needs this in the future.
     */
    void deleteHandler(http_request request, const std::string key) 
    {
        std::cout << "DEL req received: " << key << std::endl;

        // check key exists
        if (this->keyBlockNodeMap.find(key) == this->keyBlockNodeMap.end())
        {
            std::cout << "DEL: failed - key doesn't exist" << std::endl;
            request.reply(status_codes::InternalError);
            return;
        }

        // Find all nodes that store at least 1 block for `key`
        std::set<int> nodeIds;
        std::shared_ptr<std::map<int, int>> blockNodeMap = this->keyBlockNodeMap[key];

        for (auto p : *(blockNodeMap))
        {
            int nodeId = p.second;
            nodeIds.insert(nodeId);
        }

        std::vector<pplx::task<void>> delBlockTasks;

        // call `deleteBlocks` for each node
        for (int nodeId : nodeIds)
        {
            auto task = deleteBlocks(nodeId, key);
            delBlockTasks.push_back(task);
        }

        bool success = true;
        auto task = pplx::when_all(delBlockTasks.begin(), delBlockTasks.end())
        .then([&](pplx::task<void> allTasks)
        {
            try
            {
                allTasks.get();
            }
            catch (const std::exception& e)
            {
                std::cout << "DEL: failed - " << e.what() << std::endl;
                success = false;
                request.reply(status_codes::InternalError);
            }
        });

        task.wait();

        if (!success)
            return;
        
        // remove key's entry from KBN entirely
        this->keyBlockNodeMap.erase(key);

        // success response
        std::cout << "DEL: successful" << std::endl;
        request.reply(status_codes::OK);
        return;
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
        auto p = ApiUtils::parsePath(request.relative_uri().to_string());
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

        try 
        {
            // start request listener
            listener
                .open()
                .then([&addr](){ std::cout << "Master server is listening at: " << addr << std::endl; });
            
            // start a thread to periodically check storage node health
            std::thread nodeHealthThread([this](){
                this->checkNodeHealth();
            });
            nodeHealthThread.detach();

            while (1);
        } 
        catch (const std::exception& e) 
        {
            std::cout << "An error occurred: " << e.what() << std::endl;
        }
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
            auto p = ApiUtils::parsePath(paths[i]);
            ASSERT_THAT(p == expectedParsedPaths[i]);
        }
    }

    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "MasterServer Tests" << std::endl;
        std::cerr << "###################################" << std::endl;

        std::vector<std::pair<std::string, std::function<void()>>> tests = {
            TEST(testParsePath)
        };

        for (auto &[name, func] : tests)
        {
            TestUtils::runTest(name, func);
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

    // MasterServerTests::runAll();
}

int main() 
{
    run();
}

/*
TODO:
    - replication
    - adding / removing nodes 
    - master restarting
        - i.e. if it goes down, it needs to re-build its view of the world
    - understand and internalise CAP
    - implement concurrent r/w protections for DiskStorage
        - see bottom of server.cpp for plan
    - make .then() code non-blocking (related to concurrent r/w)
    - group together and separate out handlers into nice abstraction
    
    - authenticate requests from master -> server
        - token or generated API key
    - sort out documentation
    - potentially:
        - return JSON
        - reason:
            - if CURL is too clunky, might want to write our own lightweight client
            - in that case, MasterServer could return JSON, and our client process
              that JSON.
            - for now, CURL is great, its just something to think about

WITH REPLICATION PLAN:
    GET:
        - KBN: { key -> {blockNum -> nodeIdList} }
            - nodeIdList.size() == R, where R is our replication factor
        - for each block, greedily pick first 'healthy' node (as per latest health check)
        - build up {nodeId -> blockNumList}
            - so can query node's blocks in one request
        - send storage a GET, with blockNumList as payload
            - storage node will obviously read all `key`s blocks
            - will just return those it was asked for
    
    WRITE:
        - for each block:
            - hash(key + blockNum) -> R nodes to store on
            - i.e. KBN[key][blockNum] = {node0, node1, ... nodeR}
        - {blockNum -> nodeList} map to build up {nodeId -> blockNumList} map
        - for each nodeId:
            - build up vector<Block> blocks
            - call sendBlocks(nodeId, key, blocks)

ADDING / REMOVING NODES PLAN:
    - want to NOTIFY MasterServer of a new node in via an authenticated API call
    - real challenge is re-allocation
    - also need to figure out how actually tell MasterServer
        that another storage server exists
    - we can easily spin up another storage server
        - but do we manually set its ID?
        - we ideally want to do this quickly and in an automated fashion
    - plan: 
        - /register-node:POST endpoint
        - authenticated by an API key / token
        - gives ip:port and nodeId
        - then, MasterServer performs the re-balancing
            - how to deal with the downtime of this? (see CAP)

MASTER RESTARTING:
    - either:
        - persist master's critical state to disk
            - simple, small format
        - completely re-build by querying storage nodes
*/