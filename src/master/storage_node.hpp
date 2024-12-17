#pragma once
#include <string>
#include "hash_ring.hpp"

/**
 * Represents a storage node.
 */
class StorageNode {
private:
    /**
     * Generates unique PhysicalNode ids by incrementing on 
     * each constructor call
     */
    static int idGenerator;

    /**
     * Create all virtual nodes. 
     */
    void createVirtualNodes();

public:
    /**
     * Unique id given to physical node
     */
    int id;

    /**
     * ip:port combination of physical node
     */
    std::string ipPort;

    int numVirtualNodes;
    
    /**
     * Physical node's virtual nodes
     */
    std::vector<std::shared_ptr<VirtualNode>> virtualNodes;

    /**
     * Param constructor
     */
    StorageNode(std::string ipPort, int numVirtualNodes);

    std::string toString();
};