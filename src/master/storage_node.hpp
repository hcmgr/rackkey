#pragma once
#include <string>
#include "hash_ring.hpp"

/**
 * Represents statistics of a single storage node.
 */
struct StorageNodeStats
{
    uint32_t blocksStored;
    uint32_t dataBytesUsed;
    uint32_t dataBytesFree;
    uint32_t dataBytesTotal;

    /* Default constructor */
    StorageNodeStats()
        : blocksStored(0),
          dataBytesUsed(0),
          dataBytesFree(0),
          dataBytesTotal(0)
    {
    }
};

/**
 * Represents a storage node.
 */
class StorageNode {
private:
    /**
     * Generates unique StorageNode ids by incrementing on 
     * each constructor call
     */
    static int idGenerator;

    /**
     * Create all virtual nodes. 
     */
    void createVirtualNodes(int numVirtualNodes);

public:
    /**
     * Unique id given to the storage node
     */
    int id;

    /**
     * ip:port combination of the storage node
     */
    std::string ipPort;

    /**
     * Denotes whether our node is `healthy`, as per the last `health check`
     * performed by master (see MasterServer::checkNodeHealth())
     */
    bool isHealthy;

    /* Node statistics */
    StorageNodeStats stats;

    /**
     * Storage node's virtual nodes for the hash ring
     */
    std::vector<std::shared_ptr<VirtualNode>> virtualNodes;

    /* Param constructor */
    StorageNode(std::string ipPort, int numVirtualNodes);

    /* Returns human-readable representation of the storage node */
    std::string toString();
};

