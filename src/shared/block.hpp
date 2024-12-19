#pragma once

#include <vector>
#include <string>
#include <unordered_set>

/**
 * Represents an I/O block.
 * 
 * NOTE: Block objects only help organise I/O and are NOT kept 
 *       around in memory. i.e. they last the duration of the
 *       GET/PUT request.
 */
class Block {
public:
    std::string key;
    uint32_t blockNum;

    uint32_t dataSize;
    std::vector<unsigned char>::iterator dataStart;
    std::vector<unsigned char>::iterator dataEnd;

    /**
     * Default constructor
     */
    Block();

    /**
     * Parameterised constructor
     */
    Block(
        std::string key,
        uint32_t blockNum,
        uint32_t dataSize,
        std::vector<unsigned char>::iterator dataStart,
        std::vector<unsigned char>::iterator dataEnd
    );

    /**
     * Serializes the block into the given byte array
     */
    void serialize(std::vector<unsigned char>& outputBuffer);

    /**
     * Deserialize block buffer into vector of Block objects
     */
    static std::vector<Block> deserialize(std::vector<unsigned char>& inputBuffer);

    /**
     * Checks all meta data fields AND full data section for equality.
     */
    bool equals(Block &other);

    /**
     * Returns the string represenation of a Block.
     * 
     * NOTE: 
     * 
     * By default, we only show block meta data (i.e. key, block num and data size).
     * On showData == true, raw block data is also shown.
     */
    std::string toString(bool showData = false);
};


namespace BlockUtils
{
    /**
     * Generate `N` blocks with total data size `numBytes`, each with
     * key `key`.
     * 
     * Returns pair of the form {block list, block numbers}
     * 
     * NOTE: used to write tests for Block and other modules.
     */
    std::pair<std::vector<Block>, std::unordered_set<uint32_t>> generateRandom(
        std::string key, 
        uint32_t blockSize,
        uint32_t numBytes,
        std::vector<std::vector<unsigned char>> &dataBuffers
    );
};

namespace BlockTests
{
    void testBlockSerializeDeserialize();
};