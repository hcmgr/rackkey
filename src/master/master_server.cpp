#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <mutex>

#include "storage_node.hpp"
#include "hash_ring.hpp"
#include "utils.hpp"
#include "config.hpp"
#include "block.hpp"
#include "test_utils.hpp"
#include "master_config.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;
using namespace web::http::experimental::listener;

class MasterServer 
{
public:
    /**
     * HashRing used to distribute our blocks evenly across nodes
     */
    HashRing hashRing;

    /**
     * Stores mapping of the form: key -> {block num -> storage node id list}.
     * 
     * i.e. for each KEY, we store a mapping from block number to a list of storage node ids.
     * 
     * Allows for fast lookup of block location.
     * 
     * NOTE: 
     * 
     * Nicknamed the 'KBN' for brevity.
     */
    std::map<std::string, std::shared_ptr<std::map<uint32_t, std::vector<uint32_t>>>> keyBlockNodeListMap;

    /**
     * Stores our storage nodes.
     * 
     * Mapping is of the form: storage node id -> StorageNode object.
     */
    std::map<uint32_t, std::shared_ptr<StorageNode>> storageNodes;

    /**
     * Stores currently open connections to storage nodes.
     * 
     * Mapping is of the form: storage node id -> http_client object.
     */
    std::map<uint32_t, std::shared_ptr<http_client>> openConnections;
    std::mutex openConnectionsMutex;

    /**
     * Stores 'health status' of nodes, as per last health check.
     * 
     * Mapping is of the form: storage node id -> 'health status' boolean
     */
    std::map<uint32_t, bool> nodeHealthMap;
    std::mutex nodeHealthMapMutex;

    MasterConfig config;

    /* Default constructor */
    MasterServer(std::string configFilePath) 
        : hashRing(),
          keyBlockNodeListMap(),
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
        std::map<uint32_t, uint32_t> nodeBlockCounts; // node id -> block count
        
        std::map<uint32_t, std::vector<uint32_t>> blockNodeListMap = *(this->keyBlockNodeListMap[key]);
        uint32_t totalNumBlocks = blockNodeListMap.size();
        uint32_t totalNumReplicatedBlocks = 0;

        for (auto p: blockNodeListMap)
        {
            uint32_t blockNum = p.first; 
            std::vector<uint32_t> nodeIds = p.second;

            for (auto nodeId : nodeIds)
            {
                if (nodeBlockCounts.find(nodeId) == nodeBlockCounts.end())
                    nodeBlockCounts[nodeId] = 0;
                nodeBlockCounts[nodeId]++;
                totalNumReplicatedBlocks++;
            }
        }

        std::ostringstream oss;

        oss << "\n";
        oss << "-------------------\n";
        oss << "key: " << key << "\n";
        oss << "unique blocks: " << totalNumBlocks << "\n";
        oss << "total blocks (including replicas): " << totalNumReplicatedBlocks << "\n";
        oss << "block distribution:\n";
        oss << "{\n";
        for (const auto& [nodeId, blockCount] : nodeBlockCounts) {
            oss << "  " << nodeId << ": " << blockCount << "\n";
        }
        oss << "}\n";
        oss << "-------------------\n";
        oss << "\n";

        std::cout << oss.str();
    }

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

    /**
     * Thread function used to periodically check health of all storage nodes.
     * 
     * NOTE:
     * 
     * Time between health checks (i.e. the period) is given by config.healthCheckPeriodMs 
     * (i.e. masterServer.healthCheckPeriodMs in config.json).
     */
    void checkNodeHealth()
    {
        while (1) 
        {
            uint32_t period = this->config.healthCheckPeriodMs;

            /**
             * Every `period` milliseconds, send a GET to the /health/ endpoint
             * of each storage node.
             * 
             * Update our nodeHealthMap accordingly.
             */

            std::this_thread::sleep_for(
                std::chrono::milliseconds(period)
            );

            std::vector<pplx::task<void>> healthCheckTasks;
            for (auto p : storageNodes)
            {
                uint32_t nodeId = p.first;
                std::shared_ptr<StorageNode> sn = p.second;

                auto client = getHttpClient(sn);

                // build and send request
                http_request req = http_request();
                req.set_method(methods::GET);
                req.set_request_uri(U("/health/"));

                auto task = client->request(req)
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

            bool reporting = false; // TODO : make an #ifdef thing
            if (reporting)
            {
                std::cout << std::endl << "Storage node health: " << std::endl;
                PrintUtils::printMap(this->nodeHealthMap);
            }
        }
    }

    /**
     * Retreive the http_client associated with the given storage
     * node, or create one if it doesn't exist.
     */
    std::shared_ptr<http_client> getHttpClient(std::shared_ptr<StorageNode> sn)
    {
        if (this->openConnections.find(sn->id) == this->openConnections.end())
            this->openConnections[sn->id] = std::make_shared<http_client>(U(sn->ipPort));
        return this->openConnections[sn->id];
    }

    /**
     * Retreives blocks `blockNums` for key `key` from node `storageNodeId`.
     * 
     * NOTE:
     * 
     * `blockMap` is populated by this function (see getHandler() below).
     */
    pplx::task<void> getBlocks(
        uint32_t storageNodeId,
        std::string key,
        std::vector<uint32_t> blockNums,
        std::shared_ptr<std::map<uint32_t, Block>> blockMap,
        std::shared_ptr<std::vector<unsigned char>> responsePayload
    )
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        auto client = getHttpClient(sn);

        /**
         * Serialise block numbers to send as payload.
         */
        std::vector<unsigned char> requestPayload;
        for (uint32_t bn : blockNums)
        {
            requestPayload.insert(
                requestPayload.end(),
                reinterpret_cast<unsigned char*>(&bn),
                reinterpret_cast<unsigned char*>(&bn) + sizeof(bn)
            );
        }

        // build and send request
        http_request req = http_request();
        req.set_method(methods::GET);
        req.set_request_uri(U("/store/" + key));
        req.set_body(requestPayload);

        return client->request(req)
        .then([=](http_response response)
        {
            if (response.status_code() != status_codes::OK)
            {
                throw std::runtime_error(
                    "getBlocks() failed with status: " + std::to_string(response.status_code()));
            }
            return response.extract_vector();
        })

        // deserialise payload and populate block map
        .then([=](std::vector<unsigned char> payload)
        {
            *responsePayload = payload;
            std::vector<Block> blocks = Block::deserialize(*(responsePayload));
            for (auto block : blocks)
            {
                blockMap->insert({block.blockNum, std::move(block)});
            }
        });
    }

    /**
     * /store/{KEY}: GET
     * ---
     * Requests all of {KEY}'s blocks from the storage cluster
     * and returns them in order.
     */
    void getHandler(http_request request, const std::string key) 
    {
        std::cout << "GET req received: " << key << std::endl;

        // check key exists
        if (this->keyBlockNodeListMap.find(key) == this->keyBlockNodeListMap.end())
        {
            std::cout << "GET: failed - key doesn't exist" << std::endl;
            request.reply(status_codes::InternalError);
            return;
        }

        std::shared_ptr<std::map<uint32_t, std::vector<uint32_t>>> blockNodeMap = this->keyBlockNodeListMap[key];
        
        /**
         * For each block, we chose the first healthy storage node that
         * stores it.
         * 
         * We store our 'choices' in nodeBlockMap, which is a mapping
         * of the form: {node id -> block num list}.
         */
        std::unordered_map<uint32_t, std::vector<uint32_t>> nodeBlockMap;
        for (auto p : *(blockNodeMap))        
        {
            uint32_t blockNum = p.first;
            std::vector<uint32_t> nodeIds = p.second;

            bool foundHealthy = false;
            for (auto nodeId : nodeIds)
            {
                if (this->nodeHealthMap[nodeId])
                {
                    nodeBlockMap[nodeId].push_back(blockNum);
                    foundHealthy = true;
                    break;
                }
            }

            if (!foundHealthy)
                throw std::runtime_error("Error: no healthy nodes available for block " + std::to_string(blockNum));
        }

        // mapping of the form: {block num. -> block object}
        auto blockMap = std::make_shared<std::map<uint32_t, Block>>();

        std::vector<std::shared_ptr<std::vector<unsigned char>>> responsePayloads;
        
        /**
         * Call `getBlocks` for each node and wait on all tasks 
         * to finish.
         */
        std::vector<pplx::task<void>> getBlockTasks;
        for (auto p : nodeBlockMap)
        {
            uint32_t nodeId = p.first;
            std::vector<uint32_t> blockNums = p.second;

            auto responsePayload = std::make_shared<std::vector<unsigned char>>();
            responsePayloads.push_back(responsePayload);

            auto task = getBlocks(nodeId, key, blockNums, blockMap, responsePayload);
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

        // recombine blocks in order
        std::vector<unsigned char> payloadBuffer;
        for (auto p : *(blockMap))
        {
            Block block = p.second;
            payloadBuffer.insert(payloadBuffer.end(), block.dataStart, block.dataEnd);
        }

        std::cout << "GET: successful" << std::endl;

        // send success response
        http_response response(status_codes::OK);
        response.set_body(payloadBuffer);
        request.reply(response);
        return;
    }

    /**
     * Sends the given list of blocks `blocks` for key `key` to storage
     * node `storageNodeId`.
     * 
     * NOTE:
     * 
     * `blockNodeMap` is populated in this function (see putHandler() below).
     */
    pplx::task<void> sendBlocks(
        uint32_t storageNodeId,
        std::string key,
        std::vector<Block> &blocks,
        std::shared_ptr<std::map<uint32_t, std::vector<uint32_t>>> blockNodeMap
    )
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        auto client = getHttpClient(sn);

        // populate body
        std::vector<unsigned char> payloadBuffer;
        for (auto &block : blocks)
        {
            block.serialize(payloadBuffer);
        }

        // build and send request
        http_request req = http_request();
        req.set_method(methods::PUT);
        req.set_request_uri(U("/store/" + key));
        req.set_body(payloadBuffer);

        pplx::task<void> task = client->request(req)
            .then([=](http_response response) 
            {
                if (response.status_code() != status_codes::OK) 
                {
                    throw std::runtime_error(
                        "sendBlocks() failed with status: " + std::to_string(response.status_code()));
                }
            })

            // assign blocks to node `storageNodeId`
            .then([=]()
            {
                for (auto &block : blocks)
                {
                    (*blockNodeMap)[block.blockNum].push_back(storageNodeId);
                }
            });

        return task;
    }

    /**
     * /store/{KEY}: PUT
     * ---
     * Given {KEY} and a data payload, breaks payload into blocks and 
     * distributes them across the storage cluster.
     */
    void putHandler(http_request request, const std::string key) 
    {
        std::cout << "PUT req received: " << key << std::endl;

        // Timing point: start
        auto start = std::chrono::high_resolution_clock::now();

        /**
         * blockNodeMap maps each block num. to a list of nodes that store it.
         * 
         * i.e. {block num -> [nodeA, nodeB, ...]}
         */
        auto blockNodeMap = std::make_shared<std::map<uint32_t, std::vector<uint32_t>>>();

        auto requestPayload = std::make_shared<std::vector<unsigned char>>();

        std::vector<pplx::task<void>> sendBlockTasks;
        bool success = true;

        /**
         * Break up payload data into blocks and assign each block 
         * to R storage nodes, where R is our replication factor.
         */
        pplx::task<void> task = request.extract_vector()
        .then([&](std::vector<unsigned char> payload)
        {
            *requestPayload = std::move(payload);
                  
            uint32_t payloadSize = requestPayload->size();
            uint32_t blockCnt = 0;

            std::unordered_map<uint32_t, std::vector<Block>> nodeBlockMap;

            for (uint32_t i = 0; i < payloadSize; i += config.dataBlockSize) 
            {
                // construct the block
                uint32_t blockNum = blockCnt++;
                auto blockStart = requestPayload->begin() + i;
                auto blockEnd = requestPayload->begin() + std::min(i + config.dataBlockSize, payloadSize);
                auto dataSize = blockEnd - blockStart;

                Block block(key, blockNum, dataSize, blockStart, blockEnd);

                // replication factor
                uint32_t R = std::min(this->config.replicationFactor, this->config.numStorageNodes);

                /**
                 * Find next R (replication factor) distinct physical nodes and 
                 * add the block to the nodes' block list.
                 */
                std::string hashInput = key + std::to_string(blockNum);
                uint32_t hash = Crypto::sha256_32(hashInput);
                std::unordered_set<uint32_t> usedPhysicalNodes;
                int cnt = 0;

                while (cnt < R)
                {
                    std::shared_ptr<VirtualNode> vn = this->hashRing.findNextNode(hash);
                    uint32_t nodeId = vn->physicalNodeId;

                    if (
                        usedPhysicalNodes.find(nodeId) == usedPhysicalNodes.end() &&    // node is un-used by block
                        this->nodeHealthMap[nodeId]     // node is healthy
                       )
                    {
                        nodeBlockMap[nodeId].push_back(block);
                        usedPhysicalNodes.insert(nodeId);
                        cnt++;
                    }

                    hash = vn->hash();
                }
            }

            return nodeBlockMap;
        })

        /**
         * Send each storage node its block list
         */
        .then([&](std::unordered_map<uint32_t, std::vector<Block>> nodeBlockMap)
        {
            auto sendStart = std::chrono::high_resolution_clock::now();

            for (auto p : nodeBlockMap)
            {
                uint32_t storageNodeId = p.first;
                std::vector<Block> blocks = p.second;

                auto task = sendBlocks(storageNodeId, key, blocks, blockNodeMap);
                sendBlockTasks.push_back(task);
            }

            // wait for all `sendBlocks` tasks to finish
            return pplx::when_all(sendBlockTasks.begin(), sendBlockTasks.end())
            .then([&](pplx::task<void> allTasks)
            {
                try
                {
                    allTasks.get();
                    this->keyBlockNodeListMap[key] = blockNodeMap;
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

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Total Time: " << duration.count() << " ms" << std::endl;

        std::cout << "PUT: successful" << std::endl;

        // send success response
        http_response response(status_codes::OK);
        request.reply(response);
        return;
    }

    /**
     * Deletes all blocks correpsonding to key `key` from node `storageNodeId`.
     */
    pplx::task<void> deleteBlocks(uint32_t storageNodeId, std::string key)
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        auto client = getHttpClient(sn);

        // build and send request
        http_request req = http_request();
        req.set_method(methods::DEL);
        req.set_request_uri(U("/store/" + key));

        auto task = client->request(req)
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
     * /store/{KEY}:DEL
     * ---
     * Given {KEY}, deletes all blocks of {KEY} from the 
     * storage cluster.
     * 
     * NOTE: TODO:
     * 
     * At the moment, we don't accept specific block numbers
     * to delete. 
     * 
     * i.e. we just delete all blocks corresponding
     * to key `key`.
     * 
     * Rebalancing will likely require block-level deletion capability.
     */
    void deleteHandler(http_request request, const std::string key) 
    {
        std::cout << "DEL req received: " << key << std::endl;

        // check key exists
        if (this->keyBlockNodeListMap.find(key) == this->keyBlockNodeListMap.end())
        {
            std::cout << "DEL: failed - key doesn't exist" << std::endl;
            request.reply(status_codes::InternalError);
            return;
        }

        // find all nodes that store at least 1 block for `key`
        std::unordered_set<uint32_t> allNodeIds;
        std::shared_ptr<std::map<uint32_t, std::vector<uint32_t>>> blockNodeMap = this->keyBlockNodeListMap[key];

        for (auto p : *(blockNodeMap))
        {
            std::vector<uint32_t> nodeIds;
            for (auto nodeId : nodeIds)
                allNodeIds.insert(nodeId);
        }

        std::vector<pplx::task<void>> delBlockTasks;

        // call `deleteBlocks()` for each node
        for (uint32_t nodeId : allNodeIds)
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
        this->keyBlockNodeListMap.erase(key);

        // send success response
        std::cout << "DEL: successful" << std::endl;
        request.reply(status_codes::OK);
        return;
    }

    /**
     * /keys:GET
     * ---
     * Returns newline-separated list of all keys currently stored.
     */
    void getKeysHandler(http_request request) 
    {
        std::cout << "GET /keys req received" << std::endl;

        std::ostringstream oss;
        for (const auto &p : keyBlockNodeListMap)
            oss << p.first << "\n";
        
        request.reply(status_codes::OK, oss.str());
    }

    /**
     * /stats:GET
     * ---
     * Returns storage node statistics 
     * i.e. #blocks, #keys, space used, space free, total size, etc.
     * 
     * TODO:
     */
    void getStatsHandler(http_request request)
    {
        std::cout << "GET /stats req received" << std::endl;
        std::ostringstream oss;
        oss << config.numStorageNodes << "\n";

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
        else if (endpoint == U("/stats") && param == U(""))
        {
            if (request.method() == methods::GET)
                this->getStatsHandler(request);
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
            std::thread nodeHealthThread(&MasterServer::checkNodeHealth, this);
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
    std::string configFilePath = "../src/config.json";
    MasterServer masterServer = MasterServer(configFilePath);
    masterServer.startServer();
}

int main() 
{
    run();
}

/*
TODO:
    - try deploy to linux
    - do up some nice docs / readme stuff
        - give a chance to re-understand how all works
    - handle hash collisions
    - user should receive actual error messages
    - /stats endpoint that reports 
      num blocks stored / bytes used / max size / free
      for each storage node
        - really want to make sure the files aren't just growing
        - just stress test it with a bunch of crap and see if it grows / buckles
    - master restarting
        - i.e. minimal master persistence to rebuild from shutdown / reboot
    - adding / removing nodes / rebalancing
    - investigate distribution of key's blocks 
        - particularly want to ensure that block numbers
          are themselves evenly distributed
        - i.e. may be even, but:
            - say we have 3 nodes and r = 3
            - don't want all 3 of block0 on node0, all 3 of block1 on node1 etc
            - pretty sure this isn't the case, but want  to make sure
    - understand and internalise CAP
    - implement concurrent r/w protections for DiskStorage
        - see bottom of server.cpp for plan
    - make .then() code non-blocking (related to concurrent r/w)
    

bugs:
    - master sometimes segfaults weirdly
        - maybe when its overloaded with requests?
        - potentially have some testing where thrash it with requests
    - when num healthy nodes < R, should throw an error (doesn't right now),
        just returns junk output

potentially:
    - separate out each endpoint and their handlers into nice abstraction:
        - nested class, etc.
    - way to run all tests with one command
    - authenticate requests from master -> server
        - token or generated API key
    - if writing out more nodes in docker-compose.yml gets annoying, 
        write python script to generate one (probs makes stuff just less clear though)

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