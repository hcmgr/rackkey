#pragma once

#include <vector>
#include <string>
#include <unordered_set>

/**
 * Represents a data transmission block.
 * 
 * Data is sent/received to/from the storage cluster
 * as a list of Block objects.
 * 
 * NOTE:
 * 
 * Block objects are lightweight objects that ONLY store
 * pointers to underlying data, NOT the data itself.
 */
class Block {
public:
    /* key the block belongs to */
    std::string key;

    /* number that uniquely identifies the block within its key */
    uint32_t blockNum;

    /* size of data stored (in bytes) */
    uint32_t dataSize;

    /* start/end pointers to underlying data buffer */
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

    /**
     * Generate `N` blocks with total data size `numBytes`, each with
     * key `key`.
     * 
     * Returns pair of the form {block list, block numbers}
     * 
     * NOTE: used to write tests for Block and other modules.
     */
    static std::pair<std::vector<Block>, std::unordered_set<uint32_t>> generateRandom(
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