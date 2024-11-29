#include <iostream>
#include <map>
#include <string>
#include <memory>

#include "hash_ring.hpp"
#include "utils.hpp"
#include "crypto.hpp"

////////////////////////////////////////////
// Virtual node methods
////////////////////////////////////////////

/* Default constructor */
VirtualNode::VirtualNode(std::string id, int physicalNodeId)
    : id(id),
      physicalNodeId(physicalNodeId) {}

/* Hash function to determine virtual node's position on the ring */
uint32_t VirtualNode::hash()
{
    return Crypto::sha256_32(this->id);
}

std::string VirtualNode::toString()
{
    return this->id;
}

////////////////////////////////////////////
// PhysicalNode methods
////////////////////////////////////////////

/* Default constructor */
PhysicalNode::PhysicalNode(std::string ip, int numVirtualNodes)
    : id(idGenerator++),
      ip(ip),
      numVirtualNodes(numVirtualNodes)
{
    createVirtualNodes();
}

/**
 * Create all virtual nodes.
 * 
 * Note that virtual nodes are only added to the HashRing
 * when the physical node is added.
 */
void PhysicalNode::createVirtualNodes() 
{
    this->virtualNodes = std::vector<std::shared_ptr<VirtualNode>>(numVirtualNodes);

    std::string virtualNodeId;
    for (int i = 0; i < this->numVirtualNodes; i++) {
        virtualNodeId = this->ip + ":" + std::to_string(i);
        std::shared_ptr<VirtualNode> vn = std::make_shared<VirtualNode>(virtualNodeId, id);
        this->virtualNodes[i] = vn;
    }
}

int PhysicalNode::idGenerator = 0;

////////////////////////////////////////////
// HashRing Methods 
////////////////////////////////////////////

/* Default contructor*/
HashRing::HashRing() 
    : ring(),
      physicalNodes(),
      HASH_MODULO(UINT32_MAX) {}

/**
 * Adds a physical node to the ring.
 * 
 * In effect, this means adding all the physical node's
 * virtual nodes to the ring.
 * 
 * NOTE: this triggers a re-assignment.
 */
void HashRing::addNode(std::string ip, int numVirtualNodes) 
{
    // create and store new physical node
    std::shared_ptr<PhysicalNode> pn = std::make_shared<PhysicalNode>(ip, numVirtualNodes);
    this->physicalNodes[pn->id] = pn;

    // add all virtual nodes to the ring
    uint32_t hash;
    for (auto vn : pn->virtualNodes) {
        // compute hash
        hash = vn->hash();

        // TODO: re-assign all nodes between node and node-1 from node+1 to node

        // add vn to ring
        ring[hash] = vn;
    }
}

/**
 * Removes a physical node from the ring.
 * 
 * In effect, this means removing all the physical node's
 * virtual nodes from the ring.
 * 
 * NOTE: this triggers a re-assignment.
 */
void HashRing::removeNode(int physicalNodeId) 
{
    std::shared_ptr<PhysicalNode> pn = physicalNodes[physicalNodeId];

    // remove virtual nodes from ring
    uint32_t hash;
    for (auto vn : pn->virtualNodes) 
    {
        // TODO: re-assign all nodes between node and node-1 from node to node+1

        hash = vn->hash();
        this->ring.erase(hash);
    }

    // remove physical node altogether
    physicalNodes.erase(pn->id);
}

/* Finds and returns next node along the ring */
std::shared_ptr<VirtualNode> HashRing::findNextNode(uint32_t hash) 
{
    auto it = this->ring.upper_bound(hash);
    if (it == this->ring.end()) 
    {
        return this->ring.begin()->second;
    }
    return it->second;
}

/* Return physical node of the given id */
std::shared_ptr<PhysicalNode> HashRing::getPhysicalNode(int id)
{
    return this->physicalNodes[id];
}

/* Pretty prints all virtual nodes of the HashRing, in order */
void HashRing::prettyPrintHashRing() 
{
    int i = 0;
    for (auto p : this->ring) 
    {
        uint32_t hash = p.first;
        std::shared_ptr<VirtualNode> vn = p.second;

        std::cout << "Node : " << i++ << std::endl;
        std::cout << vn->toString() << std::endl;
        std::cout << hash << std::endl;
        std::cout << std::endl;
    }
}

////////////////////////////////////////////
// HashRing Testing
////////////////////////////////////////////

namespace HashRingTests 
{
    /**
     * Returns list of N sequential ip addresses starting from baseIP
     */
    std::vector<std::string> setupRandomIPs(std::string baseIP, int N) {
        std::vector<std::string> ips;

        size_t lastDot = baseIP.find_last_of('.');
        if (lastDot == std::string::npos) {
            std::cerr << "Invalid IP format." << std::endl;
            return ips;
        }

        std::string base = baseIP.substr(0, lastDot + 1);

        // Generate IPs starting from 0
        for (int i = 0; i < N; ++i) {
            ips.push_back(base + std::to_string(i));
        }

        return ips;
    }

    void testHashRingFindNextNode() {
        // Initialise
        HashRing hr = HashRing();
        int N = 10, M = 10;

        std::string baseIP = "192.168.0.0";
        std::vector<std::string> ips = setupRandomIPs(baseIP, N);

        for (int i = 0; i < N; i++) 
        {
            hr.addNode(ips[i], M);
        }
        hr.prettyPrintHashRing();

        // Find next node for 10 different blocks
        std::string key = "archive.zip";
        for (int i = 0; i < 10; i++) 
        {
            std::string keyBlockCombo = key + std::to_string(i);
            uint32_t hash = Crypto::sha256_32(keyBlockCombo);
            std::cout << keyBlockCombo << " : " << hash << std::endl;

            std::shared_ptr<VirtualNode> nextVn = hr.findNextNode(hash);
            std::cout << "Next node : " << nextVn->toString() << std::endl;
            std::cout << std::endl;
        }
    }

    void testHashRingEvenlyDistributed() {
        // Initialise
        HashRing hr = HashRing();
        int N = 3, M = 100;

        std::string baseIP = "192.168.0.0";
        std::vector<std::string> ips = setupRandomIPs(baseIP, N);

        for (int i = 0; i < N; i++) 
        {
            hr.addNode(ips[i], M);
        }

        // stores frequency count of each physical node (maps: id -> freq)
        std::map<int, int> physicalNodeFreqs;

        // simulate M blocks being assigned
        int numBlocks = 100000;
        std::string key = "archive.zip";
        for (int i = 0; i < numBlocks; i++) 
        {
            std::string keyBlockCombo = key + std::to_string(i);
            uint32_t hash = Crypto::sha256_32(keyBlockCombo);

            std::shared_ptr<VirtualNode> nextVn = hr.findNextNode(hash);
            int physicalNodeId = nextVn->physicalNodeId;
            if (physicalNodeFreqs.find(physicalNodeId) == physicalNodeFreqs.end())
                physicalNodeFreqs[physicalNodeId] = 0;
            physicalNodeFreqs[physicalNodeId]++;
        }

        // compute percentages
        std::map<int, float> physicalNodePercs;
        for (auto p : physicalNodeFreqs) {
            physicalNodePercs[p.first] = static_cast<float>(p.second) / numBlocks * 100;
        }

        std::cout << "Frequencies : ";
        PrintUtils::printMap(physicalNodeFreqs);
        std::cout << "Percentages : ";
        PrintUtils::printMap(physicalNodePercs);
    }
};