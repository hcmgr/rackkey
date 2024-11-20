#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <cpprest/http_client.h>
#include <iostream>
#include <map>

#include "hash_ring.hpp"
#include "utils.hpp"
#include "config.hpp"

using namespace web;
using namespace web::http;
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
     * Mapping of the form: KEY -> {blockNum -> nodeNum}.
     * 
     * i.e. for each KEY, we store a mapping from block number to node number.
     * 
     * Allows for fast lookup of block location.
     * 
     * NOTE: nicknamed the 'KBN' for brevity
     */
    std::map<std::string, std::map<int, int> > keyBlockNodeMap;

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
     * GET
     * ---
     * Given: (KEY)
     * Requests all blocks for the given KEY from the storage cluster, returns in order
     */
    void getHandler(http_request request) 
    {
        std::cout << "get req received" << std::endl;
        
        json::value responseJson = ApiUtils::createPlaceholderJson();
        
        http_response response(status_codes::OK);
        response.set_body(responseJson);
        
        request.reply(response);
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
        std::cout << "del req received" << std::endl;

        json::value responseJson = ApiUtils::createPlaceholderJson();
        
        http_response response(status_codes::OK);
        response.set_body(responseJson);
        
        request.reply(response);
    }

    void startServer() {
        uri_builder uri(this->config.masterServerIPPort);
        auto addr = uri.to_uri().to_string();
        http_listener listener(addr);

        listener.support(methods::GET, [this](http_request request) {
            this->getHandler(request);
        });
        listener.support(methods::POST, [this](http_request request) {
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