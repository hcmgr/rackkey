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
     * Unique id of the form: ip:virtualNodeNum.
     * 
     * Used as hash input to determine position on the ring.
     */
    std::string id;

    /* Physical node this virtual node points to */
    int physicalNodeId;

    /* Default constructor */
    VirtualNode(std::string id, int physicalNodeId);

    /* Hash function to determine virtual node's position on the ring */
    uint32_t hash();

    std::string toString();
};

/**
 * Represents a physical storage node
 */
class PhysicalNode {
private:
    /**
     * Generates unique PhysicalNode ids by incrementing on 
     * each constructor call
     */
    static int idGenerator;

    /**
     * Create all virtual nodes.
     * 
     * Note that virtual nodes are only added to the HashRing
     * when the physical node is added.
     */
    void createVirtualNodes();

public:
    int id;
    std::string ip;
    int numVirtualNodes;
    
    /**
     * List of physical node's virtual nodes.
     * 
     * We keep this around so that, when the node is deleted, we know
     * which virtual nodes to remove (and thus which blocks to re-assign).
     */
    std::vector<std::shared_ptr<VirtualNode>> virtualNodes;

    /* Default constructor */
    PhysicalNode(std::string ip, int numVirtualNodes);
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
     * Stores our physical storage nodes.
     * 
     * Maps unique storage node id -> PhysicalNode.
     * 
     * Allows fast lookup of a virtual node's corresponding physical node.
     */
    std::map<int, std::shared_ptr<PhysicalNode> > physicalNodes;

    const uint32_t HASH_MODULO;

public:
    /* Default constructor */
    HashRing();

    /**
     * Creates a physical node for the given ip and adds it to the ring.
     * 
     * In effect, this means adding all the physical node's
     * virtual nodes to the ring.
     * 
     * NOTE: this triggers a re-assignment.
     */
    void addNode(std::string ip, int numVirtualNodes);

    /**
     * Removes the given physical node from the ring.
     * 
     * In effect, this means removing all the physical node's
     * virtual nodes from the ring.
     * 
     * NOTE: this triggers a re-assignment.
     */
    void removeNode(int physicalNodeId);

    /* Finds and returns next (virtual) node along the ring */
    std::shared_ptr<VirtualNode> findNextNode(uint32_t hash);

    /* Return physical node of the given id */
    std::shared_ptr<PhysicalNode> getPhysicalNode(int id);

    /* Pretty prints all virtual nodes of the HashRing, in order */
    void prettyPrintHashRing();
};

namespace HashRingTests 
{
    /**
     * Returns list of N sequential ip addresses starting from baseIP
     */
    std::vector<std::string> setupRandomIPs(std::string baseIP, int N);

    void testHashRingFindNextNode();
    void testHashRingEvenlyDistributed();
};