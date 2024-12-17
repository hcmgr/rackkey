#include "storage_node.hpp"
#include "hash_ring.hpp"

////////////////////////////////////////////
// StorageNode methods
////////////////////////////////////////////

/**
 * Create all virtual nodes. 
 */
void StorageNode::createVirtualNodes()
{
    this->virtualNodes = std::vector<std::shared_ptr<VirtualNode>>(this->numVirtualNodes);

    std::string virtualNodeId;
    for (int i = 0; i < this->numVirtualNodes; i++) 
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
      numVirtualNodes(numVirtualNodes)
{
    createVirtualNodes();
}

std::string StorageNode::toString()
{
    return "storage node: " + std::to_string(id) + " (" + ipPort + ")";
}

int StorageNode::idGenerator = 0;