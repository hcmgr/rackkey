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
private:
    /**
     * Unique id of the form: ipAddr:virtualNodeNum.
     * 
     * Used as hash input to determine position on the ring.
     */
    std::string id;

    /**
     * Physical node this virtual node points to
     */
    uint32_t physicalNodeId;

public:
    VirtualNode(std::string id, uint32_t physicalNodeId)
        : id(id),
          physicalNodeId(physicalNodeId) {}
    
    uint32_t hash() 
    {
        return Crypto::sha256_32(this->id);
    }

    std::string toString() 
    {
        return this->id;
    }
}; 

/**
 * Represents a physical storage node
 */
class PhysicalNode {
private:
    static uint32_t idGenerator;

public:
    uint32_t id;
    std::string ip;
    int numVirtualNodes;
    
    /**
     * List of physical node's virtual nodes.
     * 
     * We keep this around so that, when the node is deleted, we know
     * which virtual nodes to remove (and thus which blocks to re-assign).
     */
    std::vector<std::shared_ptr<VirtualNode>> virtualNodes;

    /**
     * Create all virtual nodes.
     * 
     * Note that virtual nodes are only added to the HashRing
     * when the physical node is added.
     */
    void createVirtualNodes() {
        this->virtualNodes = std::vector<std::shared_ptr<VirtualNode>>(numVirtualNodes);

        std::string virtualNodeId;
        for (int i = 0; i < this->numVirtualNodes; i++) {
            virtualNodeId = this->ip + ":" + std::to_string(i);
            std::shared_ptr<VirtualNode> vn = std::make_shared<VirtualNode>(virtualNodeId, id);
            this->virtualNodes[i] = vn;
        }
    }

public:
    PhysicalNode(std::string ip, int numVirtualNodes)
        : id(idGenerator++),
          ip(ip),
          numVirtualNodes(numVirtualNodes)
    {
        createVirtualNodes();
    }

};
uint32_t PhysicalNode::idGenerator = 0;

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
     * Map storing our physical nodes (indexed nodes' physical id).
     * 
     * Allows fast lookup of a virtual node's corresponding physical node.
     */
    std::map<int, PhysicalNode> physicalNodes;

    const uint32_t HASH_MODULO;

public:
    HashRing() 
        : ring(),
          physicalNodes(),
          HASH_MODULO(UINT32_MAX) 
    {

    }

    ~HashRing() 
    {

    }

    /**
     * Add given node to hash ring using RingNode hash function.
     */
    void addNode(std::shared_ptr<VirtualNode> ringNode) 
    {
        uint32_t hash = ringNode->hash() % HASH_MODULO;

        // TODO: re-assign all nodes between node and node-1 from node+1 to node

        // add node to ring
        this->ring[hash] = ringNode;
        return;
    }


    /**
     * Add given node to hash ring using given hash.
     */
    void addNode(std::shared_ptr<VirtualNode> ringNode, uint32_t hash) 
    {

        // TODO: re-assign all nodes between node and node-1 from node+1 to node

        this->ring[hash] = ringNode;
        return;
    }

    void removeNode(uint32_t hash) 
    {
        // TODO: re-assign all nodes between node and node-1 from node to node+1

        // remove node from ring
        this->ring.erase(hash);
        return;
    }

    std::shared_ptr<VirtualNode> getNode(uint32_t hash) 
    {
        return this->ring[hash];
    }

    /**
     * Finds next node along from the given 'hash'.
     */
    uint32_t findNextNode(uint32_t hash) 
    {
        auto it = this->ring.upper_bound(hash);
        if (it == this->ring.end()) {
            return this->ring.begin()->first;
        }
        return it->first;
    }

    void prettyPrintHashRing() 
    {
        for (auto p : this->ring) {
            uint32_t hash = p.first;
            std::shared_ptr<VirtualNode> rn = p.second;

            std::cout << rn->toString() << std::endl;
            std::cout << hash << std::endl;
            std::cout << std::endl;
        }
    }
};

////////////////////////////////////////
/////////////// TESTING ////////////////
////////////////////////////////////////

void testPhysicalNodeInit() {
    std::string ip = "192.0.0.6";
    int N = 100;
    PhysicalNode pn = PhysicalNode(ip, N);
    for (auto vn : pn.virtualNodes) {
        std::cout << vn->toString() << std::endl;
    }
}

// void testHashRingFindNextNode() {
//     // initialise ring
//     HashRing hr = HashRing();
//     for (int i = 0; i < 10; i++) {
//         std::shared_ptr<VirtualNode> rn = std::make_shared<VirtualNode>("Monkey");
//         hr.addNode(rn);
//     }
//     hr.prettyPrintHashRing();
//     std::cout << "---" << std::endl << std::endl;

//     // Find next node for 10 different hashes
//     std::hash<std::string> hashObject;
//     std::string key = "archive.zip0";
//     for (int i = 0; i < 10; i++) {
//         std::string keyBlockCombo = key + std::to_string(i);
//         uint32_t hashDigest = Crypto::sha256_32(keyBlockCombo);
//         std::cout << keyBlockCombo << " : " << hashDigest << std::endl;

//         uint32_t nextHash = hr.findNextNode(hashDigest);
//         std::shared_ptr<VirtualNode> rn = hr.getNode(nextHash);
//         std::cout << "Next node : " << rn->toString() << std::endl;

//         std::cout << std::endl;
//     }
// }

// /**
//  * Initialise a HashRing with N equally spaced nodes
//  */
// HashRing* setupAddNEquallySpacedNodes(int N) {
//     HashRing *hr = new HashRing();
//     uint32_t hash;
//     for (int i = 0; i < N; i++) {
//         hash = i * (UINT32_MAX / N);
//         std::shared_ptr<VirtualNode> rn = std::make_shared<VirtualNode>(i, "Noders");
//         hr->addNode(rn, hash);
//     }
//     hr->prettyPrintHashRing();
//     return hr;
// }

// void testHashRingEvenlyDistributed() {
//     int N = 10;
//     HashRing *hr = setupAddNEquallySpacedNodes(N);

//     // initialise node frequency table
//     std::vector<int> nodeFreqs(N, 0);

//     // simulate M blocks being added
//     int M = 10000000;
//     std::string key = "archive.zip";
//     std::hash<std::string> hashObject;
//     for (int i = 0; i < M; i++) {
//         std::string keyBlockCombo = key + std::to_string(i);
//         uint32_t hashDigest = Crypto::sha256_32(keyBlockCombo);

//         uint32_t nextHash = hr->findNextNode(hashDigest);
//         std::shared_ptr<VirtualNode> rn = hr->getNode(nextHash);
//         nodeFreqs[rn->getId()]++;
//     }

//     // compute percentages
//     std::vector<float> nodePercs(N, 0);
//     for (int i = 0; i < N; i++) {
//         nodePercs[i] = static_cast<float>(nodeFreqs[i]) / M * 100;
//     }

//     std::cout << "Frequencies: ";
//     PrintUtils::printVector(nodeFreqs);
//     std::cout << "Percentages: ";
//     PrintUtils::printVector(nodePercs);
// }

int main() 
{
    // testHashRingFindNextNode();
    // testHashRingEvenlyDistributed();
    testPhysicalNodeInit();
}