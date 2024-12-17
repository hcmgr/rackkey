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
     * Load all config of storage nodes.
     * 
     * NOTE: is/should be called after loadConfigAsJson()
     */
    void loadStorageNodeConfig();

    /**
     * Load all config of Master server.
     * 
     * NOTE: is/should be called after loadConfigAsJson()
     */
    void loadMasterServerConfig();

public:
    /**
     * IP/Port our Master server is listening on.
     */
    std::string masterServerIPPort;

    /**
     * IP addresses of all storage nodes given in our config.json.
     */
    std::vector<std::string> storageNodeIPs;

    int numStorageNodes;

    /**
     * Time (in milliseconds) between storage node health checks.
     */
    int healthCheckPeriodMs;

    /**
     * Number of virtual hash ring nodes created for each physical node
     */
    int numVirtualNodes;

    /**
     * Storage node block size (in bytes)
     */
    int blockSize;

    /* Default constructor */
    Config(std::string configFilePath);
};

namespace ConfigTests
{
    void testConfigReadsIpAddresses();
};