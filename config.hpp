#pragma once

#include <cpprest/json.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

using namespace web;

/**
 * Represents our config as given by 'config.json'.
 */
class Config 
{
private:
    /* Path to our config.json file */
    std::string configFilePath;

    /* JSON object we read our config.json file into */
    json::value jsonConfig;

    /**
     * Load config.json into a usable JSON object.
     * 
     * NOTE: is/should be called on initilisation
     */
    void loadConfigAsJson();

    /**
     * Load the storage IP addresses.
     * 
     * NOTE: is/should be called after loadConfigAsJson()
     */
    void loadStorageNodeIPs();

public:
    /**
     * IP addresses of all storage nodes given in our config.json.
     */
    std::vector<std::string> storageNodeIPs;

    /* Default constructor */
    Config(std::string configFilePath);
};

namespace ConfigTests
{
    void testConfigReadsIpAddresses();
};