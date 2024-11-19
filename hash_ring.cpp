#include <iostream>
#include <map>
#include <string>
#include <memory>

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

    RingNode(uint32_t id, std::string name) 
    {
        this->id = id;
        this->name = name;
    }
    
    uint32_t hash() 
    {
        return hashObject(this->name + std::to_string(this->id));
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

    void addNode(std::shared_ptr<RingNode> ringNode) 
    {
        uint32_t hash = ringNode->hash() % HASH_MODULO;

        // TODO: re-assign all nodes between node and node-1 from node+1 to node

        // add node to ring
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

    /**
     * Finds next node along from the given 'hash'.
     */
    void findNextNode(uint32_t hash) 
    {
        auto next = this->ring.upper_bound(hash);
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

// TESTING //
HashRing* setupAddInitNodes() 
{
    HashRing *hr = new HashRing();
    for (int i = 0; i < 10; i++) {
        std::shared_ptr<RingNode> rn = std::make_shared<RingNode>();
        hr->addNode(rn);
    }
    return hr;
}
void testHashRingAddInitialNodes() 
{
    HashRing* hr = setupAddInitNodes();
    hr->prettyPrintHashRing();
}

void testHashRingFindNextNode() {
    HashRing* hr = setupAddInitNodes();

}

int main() 
{
    testHashRingAddInitialNodes();
}