#include <cpprest/json.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "config.hpp"

using namespace web;

////////////////////////////////////////////
// Config methods
////////////////////////////////////////////

/**
 * Load config.json into a usable JSON object.
 * 
 * NOTE: is/should be called on initilisation
 */
void Config::loadConfigAsJson() 
{
    std::ifstream fileStream(this->configFilePath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("Unable to open configuration file: " + this->configFilePath);
    }

    std::string configContent((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    this->jsonConfig = web::json::value::parse(configContent);
}

/**
 * Load all config of storage nodes.
 * 
 * NOTE: is/should be called after loadConfigAsJson()
 */
void Config::loadStorageNodeConfig() 
{
    // retreive storage node IPs
    auto nodesArray = this->jsonConfig.at(U("storageNodeIPs")).as_array();
    for (const auto& node : nodesArray) {
        this->storageNodeIPs.push_back(node.as_string());
    }
    this->numStorageNodes = this->storageNodeIPs.size();

    // retreive health check period
    this->healthCheckPeriodMs = this->jsonConfig.at(U("healthCheckPeriodMs")).as_integer();

    // retreive number of virtual nodes
    this->numVirtualNodes = this->jsonConfig.at(U("numVirtualNodes")).as_integer();

    // retreive block size
    this->blockSize = this->jsonConfig.at(U("blockSize")).as_integer();
}

/**
 * Load all config of Master server.
 * 
 * NOTE: is/should be called after loadConfigAsJson()
 */
void Config::loadMasterServerConfig()
{
    this->masterServerIPPort = this->jsonConfig.at(U("masterServerIPPort")).as_string();
}

/* Default constructor */
Config::Config(std::string configFilePath)
    : configFilePath(configFilePath),
        storageNodeIPs()
{
    loadConfigAsJson();
    loadStorageNodeConfig();
    loadMasterServerConfig();
}

////////////////////////////////////////////
// Config tests
////////////////////////////////////////////
namespace ConfigTests
{
    void testConfigReadsIpAddresses() 
    {
        std::string configFilePath = "../config.json";

        Config config = Config(configFilePath);
        std::cout << "Storage node IPs:" << std::endl;
        for (auto ip : config.storageNodeIPs)
        {
            std::cout << ip << std::endl;
        }
    }
};