#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>

#include <iostream>
#include <set>
#include <map>
#include <unordered_set>
#include <chrono>
#include <thread>
#include <mutex>
#include <regex>
#include <locale>
#include <codecvt>

#include "storage_node.hpp"
#include "hash_ring.hpp"
#include "master_config.hpp"

#include "utils.hpp"
#include "config.hpp"
#include "block.hpp"
#include "test_utils.hpp"
#include "payloads.hpp"

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
     * Stores the location of blocks on our storage cluster.
     * 
     * Mapping is of the form: { key -> {block num -> storage node id set} }.
     * 
     * i.e. for each key, we store a mapping from block number to a set 
     * of storage node ids, which represents the set of storage nodes the
     * given block is stored on.
     * 
     * NOTE: 
     * 
     * Nicknamed the 'KBN' for brevity.
     */
    std::map<std::string, std::shared_ptr<std::map<uint32_t, std::set<uint32_t>>>> keyBlockNodeMap;

    /**
     * Stores our storage nodes, which are represented as StorageNode
     * objects (see storage_node.hpp).
     * 
     * Mapping is of the form: { storage node id -> StorageNode object }.
     */
    std::map<uint32_t, std::shared_ptr<StorageNode>> storageNodes;

    /**
     * Stores currently open connections to storage nodes.
     * 
     * Mapping is of the form: { storage node id -> http_client object }.
     */
    std::map<uint32_t, std::shared_ptr<http_client>> openConnections;
    std::mutex openConnectionsMutex;

    /* Master-specific config parameters read from config.json */
    MasterConfig config;

    /* Default constructor */
    MasterServer(std::string configFilePath) 
        : hashRing(),
          keyBlockNodeMap(),
          config(configFilePath)
    {
        initialiseStorageNodes();
        // syncWithStorageNodes();
    }

    ~MasterServer() {}

    /**
     * Contains all handlers for our /store endpoint
     */
    class StoreEndpoint
    {
    private:
        MasterServer *server;
    
    public:
        explicit StoreEndpoint(MasterServer *server) : server(server) {}

        /**
         * /store/{KEY}: GET
         * ---
         * Requests all of {KEY}'s blocks from the storage cluster
         * and returns them in order.
         */
        void getHandler(http_request request, std::string key) 
        {
            std::cout << "GET req received: " << key << std::endl;

            // timing point: start
            auto start = std::chrono::high_resolution_clock::now();

            // check key exists
            if (server->keyBlockNodeMap.find(key) == server->keyBlockNodeMap.end())
            {
                std::cout << "GET: failed - key doesn't exist" << std::endl;
                request.reply(status_codes::InternalError);
                return;
            }

            std::shared_ptr<std::map<uint32_t, std::set<uint32_t>>> blockNodeMap = server->keyBlockNodeMap[key];
            
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
                std::set<uint32_t> nodeIds = p.second;

                bool foundHealthy = false;
                for (auto nodeId : nodeIds)
                {
                    std::shared_ptr<StorageNode> sn = server->storageNodes[nodeId];
                    if (sn->isHealthy)
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

            // timing point: end
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            std::cout << "Total Time: " << duration.count() << " ms" << std::endl;

            std::cout << "GET: successful" << std::endl;

            server->syncWithStorageNodes();

            // send success response
            http_response response(status_codes::OK);
            response.set_body(payloadBuffer);
            request.reply(response);
            return;
        }

        /**
         * Helper for getHandler().
         * 
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
            std::shared_ptr<StorageNode> sn = server->storageNodes[storageNodeId];

            auto client = server->getHttpClient(sn);

            /**
             * Serialize block numbers to send as payload.
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

            // deserialize payload and populate block map
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
         * /store/{KEY}: PUT
         * ---
         * Given {KEY} and a data payload, breaks payload into blocks and 
         * distributes them across the storage cluster.
         */
        void putHandler(http_request request, std::string key) 
        {
            std::cout << "PUT req received: " << key << std::endl;

            // timing point: start
            auto start = std::chrono::high_resolution_clock::now();

            /**
             * blockNodeMap maps each block num. to a list of nodes that store it.
             * 
             * i.e. {block num -> [nodeA, nodeB, ...]}
             */
            auto blockNodeMap = std::make_shared<std::map<uint32_t, std::set<uint32_t>>>();

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

                for (uint32_t i = 0; i < payloadSize; i += server->config.dataBlockSize) 
                {
                    // construct the block
                    uint32_t blockNum = blockCnt++;
                    auto blockStart = requestPayload->begin() + i;
                    auto blockEnd = requestPayload->begin() + std::min(i + server->config.dataBlockSize, payloadSize);
                    auto dataSize = blockEnd - blockStart;

                    Block block(key, blockNum, dataSize, blockStart, blockEnd);

                    // replication factor
                    uint32_t R = std::min(server->config.replicationFactor, server->config.numStorageNodes);

                    /**
                     * Find next R (replication factor) distinct storage nodes and 
                     * add the block to the nodes' block list.
                     */
                    std::string hashInput = key + std::to_string(blockNum);
                    uint32_t hash = Crypto::sha256_32(hashInput);
                    std::unordered_set<uint32_t> usedStorageNodes;
                    int cnt = 0;

                    while (cnt < R)
                    {
                        std::shared_ptr<VirtualNode> vn = server->hashRing.findNextNode(hash);
                        uint32_t nodeId = vn->physicalNodeId;
                        std::shared_ptr<StorageNode> sn = server->storageNodes[nodeId];

                        bool nodeUnusedByBlock = usedStorageNodes.find(nodeId) == usedStorageNodes.end();
                        bool nodeHealthy = sn->isHealthy;

                        if (nodeUnusedByBlock && nodeHealthy)
                        {
                            nodeBlockMap[nodeId].push_back(block);
                            usedStorageNodes.insert(nodeId);
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

                        // update kbn
                        server->keyBlockNodeMap[key] = blockNodeMap;
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

            server->calculateAndShowBlockDistribution(key);

            // timing point: end
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
         * Helper for putHandler().
         * 
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
            std::shared_ptr<std::map<uint32_t, std::set<uint32_t>>> blockNodeMap
        )
        {
            std::shared_ptr<StorageNode> sn = server->storageNodes[storageNodeId];

            auto client = server->getHttpClient(sn);

            // populate request payload
            std::vector<unsigned char> payloadBuffer;
            for (auto &block : blocks)
                block.serialize(payloadBuffer);

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

                    // assign each block to node `storageNodeId`
                    for (auto &block : blocks)
                        (*blockNodeMap)[block.blockNum].insert(storageNodeId);
                    
                    return response.extract_vector();
                })

                // update node's stats
                .then([=](std::vector<unsigned char> payload)
                {
                    // if removing existing blocks, subtract their stats contribution
                    if (server->keyBlockNodeMap.find(key) != server->keyBlockNodeMap.end())
                    {
                        uint32_t existingBlocks = 0;
                        for (auto &blockNodesPair : *(server->keyBlockNodeMap[key]))
                        {
                            std::set<uint32_t> &nodeIds = blockNodesPair.second;
                            if (nodeIds.find(sn->id) != nodeIds.end())
                                existingBlocks++;
                        }
                        sn->stats.blocksStored -= existingBlocks;
                    }

                    uint32_t blocksAdded = blocks.size();
                    sn->stats.blocksStored += blocksAdded;

                    server->updateNodeDataSizes(sn, payload);
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
        void deleteHandler(http_request request, std::string key) 
        {
            std::cout << "DEL req received: " << key << std::endl;

            // check key exists
            if (server->keyBlockNodeMap.find(key) == server->keyBlockNodeMap.end())
            {
                std::cout << "DEL: failed - key doesn't exist" << std::endl;
                request.reply(status_codes::InternalError);
                return;
            }

            // find all nodes that store at least 1 block for `key`
            std::unordered_set<uint32_t> allNodeIds;
            std::shared_ptr<std::map<uint32_t, std::set<uint32_t>>> blockNodeMap = server->keyBlockNodeMap[key];

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
            server->keyBlockNodeMap.erase(key);

            // send success response
            std::cout << "DEL: successful" << std::endl;
            request.reply(status_codes::OK);
            return;
        }

        /**
         * Helper for deleteHandler().
         * 
         * Deletes all blocks correpsonding to key `key` from node `storageNodeId`.
         */
        pplx::task<void> deleteBlocks(uint32_t storageNodeId, std::string key)
        {
            std::shared_ptr<StorageNode> sn = server->storageNodes[storageNodeId];

            auto client = server->getHttpClient(sn);

            // build and send request
            http_request request = http_request();
            request.set_method(methods::DEL);
            request.set_request_uri(U("/store/" + key));

            auto task = client->request(request)
            .then([=](http_response response)
            {
                if (response.status_code() != status_codes::OK)
                {
                    throw std::runtime_error(
                        "deleteBlocks() failed with status: " + std::to_string(response.status_code())
                    );
                }

                return response.extract_vector();
            })

            // update node's stats
            .then([=](std::vector<unsigned char> payload)
            {
                uint32_t blocksRemoved = server->keyBlockNodeMap[key]->size();
                sn->stats.blocksStored -= blocksRemoved;

                server->updateNodeDataSizes(sn, payload);
            });

            return task;
        }
    };

    /**
     * Contains all handlers for our /keys endpoint.
     */
    class KeysEndpoint
    {
    private:
        MasterServer *server;

    public:
        explicit KeysEndpoint(MasterServer *server) : server(server) {}

        /**
         * /keys:GET
         * ---
         * Returns newline-separated list of all keys currently stored.
         */
        void getHandler(http_request request) 
        {
            std::cout << "GET /keys req received" << std::endl;

            std::ostringstream oss;
            for (const auto &p : server->keyBlockNodeMap)
                oss << p.first << "\n";
            
            request.reply(status_codes::OK, oss.str());
        }
    };

    /**
     * Contains all handlers for our /stats endpoint.
     */
    class StatsEndpoint
    {
    private:
        MasterServer *server;
    
    public:
        explicit StatsEndpoint(MasterServer *server) : server(server) {}

        /**
         * /stats:GET
         * ---
         * Returns storage node statistics in the form of a terminal-printable
         * table.
         * 
         * i.e. #blocks, #keys, space used, space free, total size, etc.
         */
        void getHandler(http_request request)
        {
            std::cout << "GET /stats req received" << std::endl;

            std::ostringstream statsDisplay = createStatsDisplay();
            request.reply(status_codes::OK, statsDisplay.str());
        }

        /**
         * Creates and returns the display string showing the statistics
         * of each storage node.
         */
        std::ostringstream createStatsDisplay()
        {
            std::ostringstream oss;

            const int numColumns = 6;
            const int columnWidth = 15;
            std::string divider(columnWidth, '-');

            // top divider
            for (int i = 0; i < numColumns; i++)
            {
                oss << divider << "-";
                if (i == numColumns - 1)
                    oss << "-";
            }
            oss << "\n";

            // header row
            oss << "|" << PrintUtils::centerText("node", columnWidth)
                << "|" << PrintUtils::centerText("status", columnWidth)
                << "|" << PrintUtils::centerText("#blocks", columnWidth)
                << "|" << PrintUtils::centerText("used", columnWidth)
                << "|" << PrintUtils::centerText("free", columnWidth)
                << "|" << PrintUtils::centerText("total", columnWidth)
                << "|"
                << "\n";
            
            // middle divider 
            for (int i = 0; i < numColumns; i++)
            {
                oss << "|" << divider;
                if (i == numColumns - 1)
                    oss << "|";
            }

            oss << "\n";
            
            // print each node row
            for (auto &p : server->storageNodes)
            {
                uint32_t nodeId = p.first;
                std::shared_ptr<StorageNode> sn = p.second;

                StorageNodeStats nodeStats = sn->stats;

                std::string nodeText = std::to_string(nodeId);
                std::string nodeStatusText = sn->isHealthy ? "running" : "down";
                std::string nodeNumBlocksText = std::to_string(nodeStats.blocksStored);
                std::string nodeFreeSizeText = PrintUtils::formatNumBytes(nodeStats.dataBytesUsed);
                std::string nodeUsedSizeText = PrintUtils::formatNumBytes(nodeStats.dataBytesFree);
                std::string nodeTotalSizeText = PrintUtils::formatNumBytes(nodeStats.dataBytesTotal);

                oss << "|" << PrintUtils::centerText(nodeText, columnWidth)            
                    << "|" << PrintUtils::centerText(nodeStatusText, columnWidth)     
                    << "|" << PrintUtils::centerText(nodeNumBlocksText, columnWidth) 
                    << "|" << PrintUtils::centerText(nodeFreeSizeText, columnWidth)  
                    << "|" << PrintUtils::centerText(nodeUsedSizeText, columnWidth) 
                    << "|" << PrintUtils::centerText(nodeTotalSizeText, columnWidth) 
                    << "|"
                    << "\n";
            }

            // bottom divider
            for (int i = 0; i < numColumns; i++)
            {
                oss << divider << "-";
                if (i == numColumns - 1)
                    oss << "-";
            }

            oss << "\n";

            return oss;
        }
    };

    /**
     * Initialise our storages nodes and add their virtual nodes 
     * to the hash ring.
     */
    void initialiseStorageNodes() {
        for (std::string ipPort : this->config.storageNodeIPs) 
        {
            auto storageNode = std::make_shared<StorageNode>(ipPort, this->config.numVirtualNodes);
            this->storageNodes[storageNode->id] = storageNode;

            // add all virtual nodes to the hash ring
            for (auto &vn : storageNode->virtualNodes)
                hashRing.addNode(vn);
        }
    }

    void syncWithStorageNodes()
    {
        std::vector<pplx::task<void>> syncTasks;
        for (auto &p : this->storageNodes)
        {
            uint32_t nodeId = p.first;
            auto task = syncWithStorageNode(nodeId);
            syncTasks.push_back(task);
        }

        auto task = pplx::when_all(syncTasks.begin(), syncTasks.end());
        task.wait();
    }

    /**
     * 
     */
    pplx::task<void> syncWithStorageNode(uint32_t storageNodeId)
    {
        std::shared_ptr<StorageNode> sn = this->storageNodes[storageNodeId];

        auto client = this->getHttpClient(sn);

        http_request request;
        request.set_method(methods::GET);
        request.set_request_uri(U("/sync"));

        auto task = client->request(request)
        .then([=](http_response response)
        {
            if (response.status_code() != status_codes::OK)
            {
                throw std::runtime_error(
                    "syncWithStorageNode() failed with status: " + std::to_string(response.status_code())
                );
            }

            return response.extract_vector();
        })

        .then([=](std::vector<unsigned char> payload)
        {
            Payloads::SyncResponse syncResponse = Payloads::SyncResponse::deserialize(payload);
        });
        return task;
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
             * Update the health status of each StorageNode object accordingly.
             */

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
                .then([=](pplx::task<http_response> prevTask){
                    try
                    {
                        // can throw an http_exception if connection isn't established
                        http_response resp = prevTask.get();

                        bool isHealthy = (resp.status_code() == status_codes::OK);
                        sn->isHealthy = isHealthy;

                    }
                    catch (const http_exception &e)
                    {
                        sn->isHealthy = false;
                    }
                });

                healthCheckTasks.push_back(task);
            }

            // wait on all request tasks - we're on a separate thread so this won't hurt
            auto task = pplx::when_all(healthCheckTasks.begin(), healthCheckTasks.end());
            task.wait();

            // wait for `period` ms
            std::this_thread::sleep_for(
                std::chrono::milliseconds(period)
            );
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
     * Calculates and displays the distribution of the given `key`s blocks
     * across the storage nodes.
     */
    void calculateAndShowBlockDistribution(std::string key) 
    {
        std::map<uint32_t, uint32_t> nodeBlockCounts; // node id -> block count
        
        std::map<uint32_t, std::set<uint32_t>> blockNodeMap = *(this->keyBlockNodeMap[key]);
        uint32_t totalNumBlocks = blockNodeMap.size();
        uint32_t totalNumReplicatedBlocks = 0;

        for (auto p: blockNodeMap)
        {
            uint32_t blockNum = p.first; 
            std::set<uint32_t> nodeIds = p.second;

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
     * Updates the given storage node's data size statistics
     * from the given `sizeResponseBuffer`.
     * 
     * NOTE:
     * 
     * Master receives a 'size response' on PUT and DEL, which 
     * gives the new data sizes for that storage node after the 
     * given operation (see StorageServer docs for API details).
     */
    void updateNodeDataSizes(
        std::shared_ptr<StorageNode> sn,
        std::vector<unsigned char> &sizeResponseBuffer)
    {
        Payloads::SizeResponse sizeResponse = Payloads::SizeResponse::deserialize(sizeResponseBuffer);

        sn->stats.dataBytesUsed = sizeResponse.dataUsedSize;
        sn->stats.dataBytesTotal = sizeResponse.dataTotalSize;
        sn->stats.dataBytesFree = sizeResponse.dataTotalSize - sizeResponse.dataUsedSize;
        return;
    }

    /**
     * Routes the given `request` to the appropriate endpoint / handler.
     */
    void router(http_request request) {
        auto p = ApiUtils::parsePath(request.relative_uri().to_string());
        std::string endpoint = p.first;
        std::string param = p.second;

        StoreEndpoint storeEndpoint(this);
        KeysEndpoint keysEndpoint(this);
        StatsEndpoint statsEndpoint(this);

        if (endpoint == U("/store"))
        {
            if (request.method() == methods::GET)
                storeEndpoint.getHandler(request, param);
            if (request.method() == methods::PUT)
                storeEndpoint.putHandler(request, param);
            if (request.method() == methods::DEL)
                storeEndpoint.deleteHandler(request, param);
        }
        else if (endpoint == U("/keys") && param == U(""))
        {
            if (request.method() == methods::GET)
                keysEndpoint.getHandler(request);
        }
        else if (endpoint == U("/stats") && param == U(""))
        {
            if (request.method() == methods::GET)
                statsEndpoint.getHandler(request);
        }
        else 
        {
            std::cout << "Endpoint not implemented: " << endpoint << std::endl;
            return;
        }
    }

    /**
     * Starts the master server and starts the node health thread (see checkNodeHealth()).
     */
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
    - change all for (auto &p : ...) to destructure the pair directly
        - i.e. for (auto &[key, value])
    - make SyncResponse mapping of form: key -> BlockNumList object
    - master restarting
        - /sync endpoint on storage server that master hits on start for each server
        - make BATEntry include 'key'
    - fix 'no delete' bug
    - adding / removing nodes / rebalancing
    - generally fix up .then logic
        - not really async sometimes
        - may have expensive unintended copies when capturing
          by value
    - do up Master and Storage docs
    - handle hash collisions
        - in BAT AND on hash ring
    - user should receive actual error messages
    - investigate distribution of key's blocks 
        - particularly want to ensure that block numbers
          are themselves evenly distributed
        - i.e. may be even, but:
            - say we have 3 nodes and r = 3
            - don't want all 3 of block0 on node0, all 3 of block1 on node1 etc
            - pretty sure this isn't the case, but want  to make sure
    - indexation for store file (currently O(N) searching through all BAT entries)
    - understand and internalise CAP and how it applies here
    - implement concurrent r/w protections for DiskStorage
        - see bottom of server.cpp for plan
    - make .then() code non-blocking (related to concurrent r/w)

bugs:
    - master sometimes segfaults weirdly
        - maybe when its overloaded with requests?
        - potentially have some testing where thrash it with requests
    - when num healthy nodes < R, should throw an error (doesn't right now),
        just returns junk output

potentially / clean up:
    - separate endpoint inner classes just into own files
    - each request should have an output stream that you print
      at the end of the request (i.e. a logger)
        - i.e. endpoint, request type, time taken, other info, etc.
        - NOTE: build up throughout and print synchronously at the end
          to avoid multi-threaded-printing-weirdness
    - way to run all tests with one command
    - authenticate requests from master -> server
        - token or generated API key
    - if writing out more nodes in docker-compose.yml gets annoying, 
        write python script to generate one (probs makes stuff just less clear though)
    - official master server testing
        - mocking endpoints?
    - just pass StorageConfig to DiskStorage directly, instead of passing
      each config parameter manually

MASTER RESTARTING:
    - persist:
        - perhaps just the whole master config?
            - no
            - the idea of config.json is for it to specify
              config for the life of the program
            - wanna change the config? gonna need a restart.
                - if change num virtual nodes, or replication factor,
                  need a re-balance
            - ip:port's given in config file just supposed to be starters
              anyway -> as in, can add more dynamically as you go
            - when add nodes dynamically, shouldn't provide any way 
              to change config
                - uses same config as is already initialised by config.json
        - for each node:
            - ip:port
            - num virtual nodes
            - replication factor

    - for each node, query /sync endpoint
    - need:
        - each key and its block numbers
    - response format:
        ----
        key0
        numBlocks
        blockNumA
        blockNumB
        ...
        ----
        key1
        numBlocks
        blockNumA
        blockNumB
        ...
        ----
        ...
*/