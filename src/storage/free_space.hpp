#include <string>

#include "utils.hpp"
#include "block.hpp"
#include "crypto.hpp"

/**
 * Free space map used to find contiguous sections of blocks on disk.
 */
class FreeSpaceMap
{
public:

    /**
     * Number of blocks bitmap keeps track of
     */
    uint32_t blockCapacity;

    /**
     * Bitmap data structure used
     */
    std::vector<uint8_t> bitMap;

    /**
     * Default constructor
     */
    FreeSpaceMap(uint32_t blockCapacity);

    /**
     * TODO:
     * 
     * Finds `N` contiguous free blocks and allocates them.
     * 
     * Returns the starting block number.
     */
    std::optional<uint32_t> allocateNBlocks(uint32_t N);

    /**
     * Frees `N` contiguous blocks starting at block number `startBlockNum`.
     */
    void freeNBlocks(uint32_t startBlockNum, uint32_t N);

    /**
     * Returns string representation of the free space map.
     * 
     * NOTE:
     * 
     * By default, we only show mapped blocks.
     * Setting showUnMapped = true shows unmapped blocks.
     */
    std::string toString(bool showUnMapped = false);

private:

    /**
     * TODO:
     * 
     * Finds `N` contiguous free blocks and returns the 
     * starting block number.
     */
    std::optional<uint32_t> findNFreeBlocks(uint32_t N);

    /**
     * Allocates `count` bits of byte with index `index`, starting at `startPos`
     */
    void allocateBitsInByte(uint32_t index, uint32_t startPos, uint32_t count);

    /**
     * Frees `count` bits of byte with index `index`, starting at `startPos`
     */
    void freeBitsInByte(uint32_t index, uint32_t startPos, uint32_t count);

    /**
     * Returns true if the given block is mapped, false if its free
     */
    bool isMapped(uint32_t blockNum);
};

/**
 * Test suite for FreeSpaceMap
 */
namespace FreeSpaceMapTests
{
    void testFreeNBlocks();
    void testAllocateNBlocks();
    void testAllocateThenFree();
};
