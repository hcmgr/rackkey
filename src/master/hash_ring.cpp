#include <iostream>
#include <map>
#include <string>
#include <memory>

#include "hash_ring.hpp"
#include "utils.hpp"
#include "crypto.hpp"
#include "test_utils.hpp"

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
// HashRing Methods 
////////////////////////////////////////////

/* Default contructor*/
HashRing::HashRing() 
    : ring() {}

/**
 * Adds a physical node to the ring.
 * 
 * In effect, this means adding all the physical node's
 * virtual nodes to the ring.
 * 
 * NOTE: this triggers a re-assignment.
 */
void HashRing::addNode(std::shared_ptr<VirtualNode> virtualNode) 
{
    uint32_t hash = virtualNode->hash();
    ring[hash] = virtualNode;
}

/**
 * Removes a physical node from the ring.
 */
void HashRing::removeNode(std::shared_ptr<VirtualNode> virtualNode) 
{
    uint32_t hash = virtualNode->hash();
    ring.erase(hash);
}

/**
 * Return the number of VirtualNode's on the ring.
 */
int HashRing::getNodeCount()
{
    return this->ring.size();
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

/* Pretty prints all virtual nodes of the HashRing, in order */
void HashRing::prettyPrintHashRing() 
{
    int i = 0;
    for (auto p : this->ring) 
    {
        uint32_t hash = p.first;
        std::shared_ptr<VirtualNode> virtualNode = p.second;

        std::cout << "vnode: " << i++ << std::endl;
        std::cout << virtualNode->toString() << std::endl;
        std::cout << hash << std::endl;
        std::cout << std::endl;
    }
}

////////////////////////////////////////////
// HashRing Testing
////////////////////////////////////////////

namespace HashRingTests 
{
    void testHashRingFindNextNode() {
        // Initialise HashRing
        HashRing hr = HashRing();
        int numPhysicalNodes = 3;
        int numVirtualNodes = 10;

        std::string ipPrefix = "127.0.0.1:";

        std::vector<std::pair<uint32_t, std::shared_ptr<VirtualNode>>> sortedNodes;

        for (int i = 0; i < numPhysicalNodes; i++) 
        {
            std::string ipPort = ipPrefix + std::to_string(i);

            for (int j = 0; j < numVirtualNodes; j++) 
            {
                std::string vnodeId = ipPort + std::to_string(j);
                auto virtualNode = std::make_shared<VirtualNode>(vnodeId, i);

                hr.addNode(virtualNode);

                // store the hash of the virtual node
                uint32_t hash = Crypto::sha256_32(vnodeId);
                sortedNodes.emplace_back(hash, virtualNode);
            }
        }

        ASSERT_THAT(hr.getNodeCount() == numPhysicalNodes * numVirtualNodes);

        // sort the nodes by hash to simulate the hash ring order
        std::sort(sortedNodes.begin(), sortedNodes.end(), 
            [](const auto &a, const auto &b) { return a.first < b.first; });

        // find next node for 10 different keys and validate correctness
        std::string key = "archive.zip";
        for (int i = 0; i < 10; i++) {
            std::string keyBlockCombo = key + std::to_string(i);
            uint32_t keyHash = Crypto::sha256_32(keyBlockCombo);

            std::cout << keyBlockCombo << " : " << keyHash << std::endl;

            // Find next node using the hash ring
            std::shared_ptr<VirtualNode> nextVn = hr.findNextNode(keyHash);
            ASSERT_THAT(nextVn != nullptr);

            // manually find expected next node
            auto it = std::upper_bound(sortedNodes.begin(), sortedNodes.end(), keyHash, 
                [](uint32_t hash, const auto &node) { return hash < node.first; });

            std::shared_ptr<VirtualNode> expectedNode;
            if (it != sortedNodes.end()) {
                expectedNode = it->second;
            } else {
                expectedNode = sortedNodes.front().second;
            }

            ASSERT_THAT(nextVn->toString() == expectedNode->toString());

            std::cout << "Expected next node : " << expectedNode->toString() << std::endl;
            std::cout << "Actual next node   : " << nextVn->toString() << std::endl;
            std::cout << std::endl;
        }
    }

    void testHashRingEvenlyDistributed() {
        // Initialise
        HashRing hr = HashRing();
        int numPhysicalNodes = 5;
        int numVirtualNodes = 100;

        std::string ipPrefix = "127.0.0.1:";
        
        for (int i = 0; i < numPhysicalNodes; i++)
        {
            std::string ipPort = ipPrefix + std::to_string(i);

            for (int j = 0; j < numVirtualNodes; j++)
            {
                std::string vnodeId = ipPort + std::to_string(j);
                auto storageNode = std::make_shared<VirtualNode>(vnodeId, i);
                hr.addNode(storageNode);
            }
        }

        ASSERT_THAT(hr.getNodeCount() == numPhysicalNodes * numVirtualNodes);

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

        // check that percentages are evenly distributed
        float expectedPercentage = 100.0 / numPhysicalNodes;
        float epsilon = 5; // 5% tolerance

        for (const auto& [nodeId, percentage] : physicalNodePercs) {
            ASSERT_THAT(std::abs(percentage - expectedPercentage) <= epsilon);
        }
        return;
    }

    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "HashRing Tests" << std::endl;
        std::cerr << "###################################" << std::endl;

        std::vector<std::pair<std::string, std::function<void()>>> tests = {
            TEST(testHashRingFindNextNode),
            TEST(testHashRingEvenlyDistributed)
        };

        for (auto &[name, func] : tests)
        {
            TestUtils::runTest(name, func);
        }
    }
};