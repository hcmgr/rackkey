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
     * Unique id of the form: ipAddr:virtualNodeNum.
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
     * Map storing our physical nodes (indexed by their unique id).
     * 
     * Allows fast lookup of a virtual node's corresponding physical node.
     */
    std::map<int, std::shared_ptr<PhysicalNode> > physicalNodes;

    const uint32_t HASH_MODULO;

public:
    /* Default constructor */
    HashRing();

    /**
     * Adds a physical node to the ring.
     * 
     * In effect, this means adding all the physical node's
     * virtual nodes to the ring.
     * 
     * NOTE: this triggers a re-assignment.
     */
    void addNode(std::string ip, int numVirtualNodes);

    /**
     * Removes a physical node from the ring.
     * 
     * In effect, this means removing all the physical node's
     * virtual nodes from the ring.
     * 
     * NOTE: this triggers a re-assignment.
     */
    void removeNode(int physicalNodeId);

    /* Finds and returns next node along the ring */
    std::shared_ptr<VirtualNode> findNextNode(uint32_t hash);

    /* Pretty prints all virtual nodes of the HashRing, in order */
    void prettyPrintHashRing();
};