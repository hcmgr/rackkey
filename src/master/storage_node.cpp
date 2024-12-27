#include "storage_node.hpp"
#include "hash_ring.hpp"

////////////////////////////////////////////
// StorageNode methods
////////////////////////////////////////////

/**
 * Create all virtual nodes. 
 */
void StorageNode::createVirtualNodes(int numVirtualNodes)
{
    virtualNodes = std::vector<std::shared_ptr<VirtualNode>>(numVirtualNodes);

    std::string virtualNodeId;
    for (int i = 0; i < numVirtualNodes; i++) 
    {
        virtualNodeId = this->ipPort + ":" + std::to_string(i);

        auto vn = std::make_shared<VirtualNode>(virtualNodeId, this->id);
        this->virtualNodes[i] = vn;
    }
}

/* Param constructor */
StorageNode::StorageNode(std::string ipPort, int numVirtualNodes)
    : id(idGenerator++),
      ipPort(ipPort),
      isHealthy(false)
{
    createVirtualNodes(numVirtualNodes);
}

std::string StorageNode::toString()
{
    return "storage node: " + std::to_string(id) + " (" + ipPort + ")";
}

int StorageNode::idGenerator = 0;