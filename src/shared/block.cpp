#include <iostream>
#include <string>
#include <random>

#include "block.hpp"
#include "utils.hpp"

////////////////////////////////////////////
// Block methods
////////////////////////////////////////////

/**
 * Default constructor
 */
Block::Block(
    std::string key,
    int blockNum,
    int dataSize,
    std::vector<unsigned char>::iterator dataStart,
    std::vector<unsigned char>::iterator dataEnd
) 
    : key(key),
        blockNum(blockNum),
        dataSize(dataSize),
        dataStart(dataStart),
        dataEnd(dataEnd)
{
}

/**
 * Pretty print Block metadata.
 * 
 * On showData == true, block data is also printed.
 */
void Block::prettyPrint(bool showData = false) 
{
    std::cout << "key: " << key << std::endl;
    std::cout << "block num: " << blockNum << std::endl;
    std::cout << "size: " << dataSize << " bytes" << std::endl;
    if (showData)
    {
        std::cout << "Data: ";
        for (auto it = dataStart; it != dataEnd; it++)
        {
            std::cout << static_cast<int>(*it) << " ";
        }
        std::cout << std::endl;
    }
}

/**
 * Serializes the block into the given byte array
 */
void Block::serialize(std::vector<unsigned char>& outputBuffer)
{
    // serialize `key`s length, along with its value
    int keySize = key.size();
    outputBuffer.insert(outputBuffer.end(), reinterpret_cast<const unsigned char*>(&keySize), 
                reinterpret_cast<const unsigned char*>(&keySize) + sizeof(keySize)); // 4 bytes
    outputBuffer.insert(outputBuffer.end(), key.begin(), key.end()); // variable

    // serialize `blockNum`
    outputBuffer.insert(outputBuffer.end(), reinterpret_cast<const unsigned char*>(&blockNum), 
                reinterpret_cast<const unsigned char*>(&blockNum) + sizeof(blockNum)); // 4 bytes

    // // serialize `dataSize`
    outputBuffer.insert(outputBuffer.end(), reinterpret_cast<const unsigned char*>(&dataSize), 
                reinterpret_cast<const unsigned char*>(&dataSize) + sizeof(dataSize)); // 4 bytes

    // serialize the data in the range [start, end)
    outputBuffer.insert(outputBuffer.end(), this->dataStart, this->dataEnd); // variable
}

/**
 * Deserialize block buffer into vector of Block objects
 */
std::vector<Block> Block::deserialize(std::vector<unsigned char>& inputBuffer) 
{
    std::vector<Block> blocks;
    auto it = inputBuffer.begin();

    while (it != inputBuffer.end()) {
        // Deserialize `keySize`
        int keySize;
        std::memcpy(&keySize, &(*it), sizeof(keySize));
        it += sizeof(keySize);

        // Deserialize `key`
        std::string key(it, it + keySize);
        it += keySize;

        // Deserialize `blockNum`
        int blockNum;
        std::memcpy(&blockNum, &(*it), sizeof(blockNum));
        it += sizeof(blockNum);

        // Deserialize `dataSize`
        int dataSize;
        std::memcpy(&dataSize, &(*it), sizeof(dataSize));
        it += sizeof(dataSize);

        // Determine `dataStart` and `dataEnd`
        auto dataStart = it;
        auto dataEnd = it + dataSize;

        // Advance the iterator to the end of the data
        it = dataEnd;

        // Construct the Block and add to the result
        blocks.emplace_back(key, blockNum, dataSize, dataStart, dataEnd);
    }

    return blocks;
}


////////////////////////////////////////////
// Block tests
////////////////////////////////////////////
namespace BlockTests
{
    void testBlockSerializeDeserialize() {
        // Allocate a container for data buffers
        std::vector<std::vector<unsigned char>> dataBuffers;

        // Create random data for each block
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 128);

        std::vector<Block> blocks;
        int N = 3;

        for (int i = 0; i < N; i++) {
            // Generate a random buffer of size 5-10 bytes
            size_t bufferSize = 5 + (i % 6);
            dataBuffers.emplace_back(bufferSize);

            // Fill the buffer with random data
            for (size_t j = 0; j < bufferSize; j++) {
                dataBuffers.back()[j] = static_cast<unsigned char>(dis(gen));
            }

            // Create a block with the random data
            blocks.emplace_back(
                "block" + std::to_string(i),
                i,
                dataBuffers.back().size(),
                dataBuffers.back().begin(),
                dataBuffers.back().end()
            );
        }

        // Pretty print the blocks before serialization
        std::cout << "Before Serialization:" << std::endl << std::endl;
        for (Block block : blocks) {
            block.prettyPrint(true);
            std::cout << std::endl;
        }

        // Serialize the blocks into a buffer
        std::vector<unsigned char> outputBuffer;
        for (Block block : blocks) {
            block.serialize(outputBuffer);
        }

        // Deserialize the buffer back into blocks
        std::vector<Block> deserializedBlocks = Block::deserialize(outputBuffer);

        // Pretty print the blocks after deserialization
        std::cout << "After Deserialization:" << std::endl << std::endl;
        for (Block block : deserializedBlocks) {
            block.prettyPrint(true);
            std::cout << std::endl;
        }
    }
}
