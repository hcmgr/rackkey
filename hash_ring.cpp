#include <iostream>
#include <map>
#include <string>
#include <memory>

#include "utils.hpp"
#include "crypto.hpp"

/**
 * Represents a node on our ring
 */
class RingNode 
{
private:
    uint32_t id;
    std::string name;

    static uint32_t idGenerator;
    std::hash<std::string> hashObject;
    
public:
    RingNode() 
    {
        this->id = idGenerator++;
        this->name = "Node";
    }

    RingNode(std::string name) {
        this->id = idGenerator++;
        this->name = name;
    }

    RingNode(uint32_t id, std::string name) 
    {
        this->id = id;
        this->name = name;
    }

    uint32_t getId() { return this->id; }
    std::string getName() { return this->name; }
    
    uint32_t hash() 
    {
        // return hashObject(this->name + std::to_string(this->id));
        return Crypto::sha256_32(this->name + std::to_string(this->id));
    }

    std::string toString() 
    {
        return this->name + std::to_string(this->id);
    }
}; 
uint32_t RingNode::idGenerator = 0;;

/**
 * Represents a hash ring used for consistent hashing
 */
class HashRing 
{
private:
    std::map<uint32_t, std::shared_ptr<RingNode> > ring;
    const uint32_t HASH_MODULO;

public:
    HashRing() 
        : ring(),
          HASH_MODULO(UINT32_MAX) 
    {

    }

    ~HashRing() 
    {

    }

    /**
     * Add given node to hash ring using RingNode hash function.
     */
    void addNode(std::shared_ptr<RingNode> ringNode) 
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
    void addNode(std::shared_ptr<RingNode> ringNode, uint32_t hash) 
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

    std::shared_ptr<RingNode> getNode(uint32_t hash) 
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
            std::shared_ptr<RingNode> rn = p.second;

            std::cout << rn->toString() << std::endl;
            std::cout << hash << std::endl;
            std::cout << std::endl;
        }
    }
};

////////////////////////////////////////
/////////////// TESTING ////////////////
////////////////////////////////////////

void testHashRingFindNextNode() {
    // initialise ring
    HashRing hr = HashRing();
    for (int i = 0; i < 10; i++) {
        std::shared_ptr<RingNode> rn = std::make_shared<RingNode>("Monkey");
        hr.addNode(rn);
    }
    hr.prettyPrintHashRing();
    std::cout << "---" << std::endl << std::endl;

    // Find next node for 10 different hashes
    std::hash<std::string> hashObject;
    std::string key = "archive.zip0";
    for (int i = 0; i < 10; i++) {
        std::string keyBlockCombo = key + std::to_string(i);
        // uint32_t hashDigest = hashObject(keyBlockCombo);
        uint32_t hashDigest = Crypto::sha256_32(keyBlockCombo);
        std::cout << keyBlockCombo << " : " << hashDigest << std::endl;

        uint32_t nextHash = hr.findNextNode(hashDigest);
        std::shared_ptr<RingNode> rn = hr.getNode(nextHash);
        std::cout << "Next node : " << rn->toString() << std::endl;

        std::cout << std::endl;
    }
}

/**
 * Initialise a HashRing with N equally spaced nodes
 */
HashRing* setupAddNEquallySpacedNodes(int N) {
    HashRing *hr = new HashRing();
    uint32_t hash;
    for (int i = 0; i < N; i++) {
        hash = i * (UINT32_MAX / N);
        std::shared_ptr<RingNode> rn = std::make_shared<RingNode>(i, "Noders");
        hr->addNode(rn, hash);
    }
    hr->prettyPrintHashRing();
    return hr;
}

void testHashRingEvenlyDistributed() {
    int N = 10;
    HashRing *hr = setupAddNEquallySpacedNodes(N);

    // initialise node frequency table
    std::vector<int> nodeFreqs(N, 0);

    // simulate M blocks being added
    int M = 10000000;
    std::string key = "archive.zip";
    std::hash<std::string> hashObject;
    for (int i = 0; i < M; i++) {
        std::string keyBlockCombo = key + std::to_string(i);
        // uint32_t hashDigest = hashObject(keyBlockCombo);
        uint32_t hashDigest = Crypto::sha256_32(keyBlockCombo);

        uint32_t nextHash = hr->findNextNode(hashDigest);
        std::shared_ptr<RingNode> rn = hr->getNode(nextHash);
        nodeFreqs[rn->getId()]++;
    }

    // compute percentages
    std::vector<float> nodePercs(N, 0);
    for (int i = 0; i < N; i++) {
        nodePercs[i] = static_cast<float>(nodeFreqs[i]) / M * 100;
    }

    std::cout << "Frequencies: ";
    PrintUtils::printVector(nodeFreqs);
    std::cout << "Percentages: ";
    PrintUtils::printVector(nodePercs);

}

int main() 
{
    // testHashRingFindNextNode();
    testHashRingEvenlyDistributed();
}