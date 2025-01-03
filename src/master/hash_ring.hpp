#pragma once

#include <iostream>
#include <map>
#include <string>
#include <memory>

#include "utils.hpp"
#include "crypto.hpp"

/**
 * Represents a virtual node on our hash ring.
 */
class VirtualNode 
{
public:
    /**
     * Unique id of the form: `[ip_port]:[virtual_node_num]`, where:
     * 
     *      ip_port - ip:port combo of corresponding physical node
     *      virtual_node_num - number of this virtual node
     * 
     * This serves as the hash input to determine its position on the ring.
     */
    std::string id;

    /* Physical node this virtual node points to */
    int physicalNodeId;

    /* Default constructor */
    VirtualNode(std::string id, int physicalNodeId);

    /* Hash function to determine virtual node's position on the ring */
    uint32_t hash();

    bool equals(VirtualNode other);

    std::string toString();

};

/**
 * Represents a hash ring used for consistent hashing
 */
class HashRing 
{
private:
    /**
     * Hash map representing our hash ring.
     * 
     * Use virtual ring nodes (each of which maps to a physical node)
     * to ensure even hash distribution.
     * 
     * Hash values are within range [0, HASH_MODULO).
     */
    std::map<uint32_t, std::shared_ptr<VirtualNode> > ring;

    /**
     * Range of the hash ring is: [0, HASH_MODULO)
     */
    const uint32_t HASH_MODULO = UINT32_MAX;

public:
    /* Default constructor */
    HashRing();

    /**
     * Adds the given virtual node to the ring.
     */
    void addNode(std::shared_ptr<VirtualNode> virtualNode);

    /**
     * Removes the given virtual node from the ring.
     */
    void removeNode(std::shared_ptr<VirtualNode> virtualNode);

    /**
     * Return the number of virtual nodes on the ring.
     */
    int getNodeCount();

    /* Finds and returns next (virtual) node along the ring */
    std::shared_ptr<VirtualNode> findNextNode(uint32_t hash);

    /* Pretty prints all virtual nodes of the HashRing, in order */
    void prettyPrintHashRing();
};

namespace HashRingTests 
{
    void testHashRingFindNextNode();
    void testHashRingEvenlyDistributed();
    void runAll();
};