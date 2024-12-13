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
Block::Block()
{

}

/**
 * Parameterised constructor
 */
Block::Block(
    std::string key,
    uint32_t blockNum,
    uint32_t dataSize,
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
        uint32_t keySize;
        std::memcpy(&keySize, &(*it), sizeof(keySize));
        it += sizeof(keySize);

        // Deserialize `key`
        std::string key(it, it + keySize);
        it += keySize;

        // Deserialize `blockNum`
        uint32_t blockNum;
        std::memcpy(&blockNum, &(*it), sizeof(blockNum));
        it += sizeof(blockNum);

        // Deserialize `dataSize`
        uint32_t dataSize;
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

/**
 * Checks all meta data fields AND full data section for equality.
 */
bool Block::equals(Block &other)
{
    // check meta data
    bool metaDataEqual = (
        key == other.key &&
        blockNum == other.blockNum &&
        dataSize == other.dataSize
    );
    if (!metaDataEqual)
        return false;

    // check if data ranges are the same size
    if (std::distance(dataStart, dataEnd) != std::distance(other.dataStart, other.dataEnd))
    {
        std::cout << "Data ranges not same size" << std::endl;
        return false;
    }

    // check raw data itself
    for (auto it1 = dataStart, it2 = other.dataStart; it1 != dataEnd; ++it1, ++it2)
    {
        if (*it1 != *it2)
        {
            std::cout << "Not equal at position: " 
                      << std::distance(dataStart, it1) << std::endl;
            std::cout << *it1 << " " << *it2 << std::endl;
            return false;
        }
    }

    return true;
}


/**
 * Returns the string represenation of a Block.
 * 
 * NOTE: 
 * 
 * By default, we only show block meta data (i.e. key, block num and data size).
 * On showData == true, raw block data is also shown.
 */
std::string Block::toString(bool showData)
{
    std::ostringstream oss;

    oss << "####################" << std::endl;
    oss << "key: " << key << std::endl;
    oss << "block num: " << blockNum << std::endl;
    oss << "size: " << dataSize << " bytes" << std::endl;

    if (showData) 
    {
        oss << "Data: " << std::endl;
        for (auto it = dataStart; it != dataEnd; ++it) 
        {
            // NOTE: display data bytes as 'char' for now
            oss << static_cast<char>(*it);
        }
        oss << std::endl;
    }

    oss << "####################" << std::endl;

    return oss.str();
}

////////////////////////////////////////////
// Block utils
////////////////////////////////////////////
namespace BlockUtils
{
    /**
     * Generate `N` blocks with total data size `numBytes`, each with
     * key `key`.
     * 
     * NOTE: used to write tests for Block and other modules.
     */
    std::vector<Block> generateRandom(
        std::string key, 
        uint32_t blockSize,
        uint32_t numBytes,
        std::vector<std::vector<unsigned char>> &dataBuffers
    )
    {
        std::vector<Block> blocks;

        // create random data for blocks - upper case letters
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(65, 90);

        uint32_t blockNum = 0;
        for (uint32_t pos = 0; pos < numBytes; pos += blockSize) {
            size_t bufferSize = std::min(blockSize, numBytes - pos);
            dataBuffers.emplace_back(bufferSize);

            // Fill the buffer with random data
            for (size_t j = 0; j < bufferSize; j++) {
                dataBuffers.back()[j] = static_cast<unsigned char>(dis(gen));
            }

            // Create a block with the random data
            blocks.emplace_back(
                key,
                blockNum++,
                dataBuffers.back().size(),
                dataBuffers.back().begin(),
                dataBuffers.back().end()
            );
        }

        return blocks;
    }
}


////////////////////////////////////////////
// Block tests
////////////////////////////////////////////
namespace BlockTests
{
    void testBlockSerializeDeserialize() 
    {
        std::string key = "archive.zip";
        uint32_t N = 10;
        uint32_t blockSize = 512;
        uint32_t numBytes = N * blockSize + 40;
        std::vector<std::vector<unsigned char>> dataBuffers;

        std::vector<Block> blocks = BlockUtils::generateRandom("archive.zip", blockSize, numBytes, dataBuffers);

        // Pretty print the blocks before serialization
        std::cout << "Before Serialization:" << std::endl << std::endl;
        for (Block block : blocks) {
            std::cout << block.toString()  << std::endl;
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
            std::cout << block.toString() << std::endl;
        }
    }
}
