#pragma once
#include "config.hpp"

class MasterConfig : public Config 
{
public:
    MasterConfig(std::string configFilePath);
    void loadVariables() override;

    /**
     * IP/Port our Master server is listening on.
     */
    std::string masterServerIPPort;

    /**
     * IP addresses of all storage nodes given in our config.json.
     */
    std::vector<std::string> storageNodeIPs;
    uint32_t numStorageNodes;

    /**
     * Time (in milliseconds) between storage node health checks.
     */
    uint32_t healthCheckPeriodMs;

    /**
     * Number of virtual hash ring nodes created for each storage node
     */
    uint32_t numVirtualNodes;

    /**
     * Number of storage nodes we write each block to
     */
    uint32_t replicationFactor;

    /**
     * Size of data (in bytes) each data block (i.e. Block object) stores.
     */
    uint32_t dataBlockSize;

    /* Maximum size of key in bytes/chars */
    uint32_t keyLengthMax;
};