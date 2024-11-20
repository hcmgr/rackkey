#include <cpprest/json.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "config.hpp"

using namespace web;

// /**
//  * Represents our config as given by 'config.json'.
//  */
// class Config 
// {
// private:
//     /* Path to our config.json file */
//     std::string configFilePath;

//     /* JSON object we read our config.json file into */
//     json::value jsonConfig;

//     /**
//      * Load config.json into a usable JSON object.
//      * 
//      * NOTE: is/should be called on initilisation
//      */
//     void loadConfigAsJson() 
//     {
//         std::ifstream fileStream(this->configFilePath);
//         if (!fileStream.is_open()) {
//             throw std::runtime_error("Unable to open configuration file: " + this->configFilePath);
//         }

//         std::string configContent((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
//         this->jsonConfig = web::json::value::parse(configContent);
//     }

//     /**
//      * Load the storage IP addresses.
//      * 
//      * NOTE: is/should be called after loadConfigAsJson()
//      */
//     void loadStorageNodeIPs() 
//     {
//         // retreive number of storage nodes
//         int numStorageNodes = this->jsonConfig.at(U("numStorageNodes")).as_integer();

//         // retreive storage node IPs
//         auto nodesArray = this->jsonConfig.at(U("storageNodeIPs")).as_array();
//         if (nodesArray.size() != numStorageNodes)
//         {
//             throw std::runtime_error("Number of storage nodes given does not match 'numStorageNodes': " + this->configFilePath);
//         }

//         for (const auto& node : nodesArray) {
//             this->storageNodeIPs.push_back(node.as_string());
//         }
//     }

// public:
//     /**
//      * IP addresses of all storage nodes given in our config.json.
//      */
//     std::vector<std::string> storageNodeIPs;

//     /* Default constructor */
//     Config(std::string configFilePath)
//         : configFilePath(configFilePath),
//           storageNodeIPs()
//     {
//         loadConfigAsJson();
//         loadStorageNodeIPs();
//     }
// };

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
 * Load the storage IP addresses.
 * 
 * NOTE: is/should be called after loadConfigAsJson()
 */
void Config::loadStorageNodeIPs() 
{
    // retreive number of storage nodes
    int numStorageNodes = this->jsonConfig.at(U("numStorageNodes")).as_integer();

    // retreive storage node IPs
    auto nodesArray = this->jsonConfig.at(U("storageNodeIPs")).as_array();
    if (nodesArray.size() != numStorageNodes)
    {
        throw std::runtime_error("Number of storage nodes given does not match 'numStorageNodes': " + this->configFilePath);
    }

    for (const auto& node : nodesArray) {
        this->storageNodeIPs.push_back(node.as_string());
    }
}

/* Default constructor */
Config::Config(std::string configFilePath)
    : configFilePath(configFilePath),
        storageNodeIPs()
{
    loadConfigAsJson();
    loadStorageNodeIPs();
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