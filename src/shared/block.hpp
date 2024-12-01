#pragma once

#include <vector>
#include <string>

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
    int blockNum;

    int dataSize;
    std::vector<unsigned char>::iterator dataStart;
    std::vector<unsigned char>::iterator dataEnd;

    /**
     * Default constructor
     */
    Block(
        std::string key,
        int blockNum,
        int dataSize,
        std::vector<unsigned char>::iterator dataStart,
        std::vector<unsigned char>::iterator dataEnd
    );

    /**
     * Pretty print Block metadata.
     * 
     * On showData == true, block data is also printed.
     */
    void prettyPrint(bool showData);

    /**
     * Serializes the block into the given byte array
     */
    void serialize(std::vector<unsigned char>& outputBuffer);

    /**
     * Deserialize block buffer into vector of Block objects
     */
    static std::vector<Block> deserialize(std::vector<unsigned char>& inputBuffer);
};

namespace BlockTests
{
    void testBlockSerializeDeserialize();
};