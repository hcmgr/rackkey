#pragma once

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
     * Default constructor - allocates a map with 0 block capacity.
     * 
     * NOTE:
     * 
     * Call initialise() to create a new map with non-zero block capacity.
     */
    FreeSpaceMap();

    /**
     * Param. constructor - allocates a map with `blockCapacity` capacity.
     * 
     * NOTE:
     * 
     * Equivalent to calling:
     *      FreeSpaceMap fsm;
     *      fsm.initialise(N); // for some arbitrary N
     */
    FreeSpaceMap(uint32_t blockCapacity);

    /**
     * Initialises a fresh free space map with capacity to hold
     * `blockCapacity` blocks.
     */
    void initialise(uint32_t blockCapacity);

    /**
     * Finds `N` contiguous free blocks and returns the 
     * starting block number.
     */
    std::optional<uint32_t> findNFreeBlocks(uint32_t N);

    /**
     * Allocates `N` contiguous blocks starting at block number `startBlockNum`.
     */
    std::optional<uint32_t> allocateNBlocks(uint32_t startBlockNum, uint32_t N);

    /**
     * Frees `N` contiguous blocks starting at block number `startBlockNum`.
     */
    void freeNBlocks(uint32_t startBlockNum, uint32_t N);

    /**
     * Returns true if the given block is mapped, false if its free
     */
    bool isMapped(uint32_t blockNum);

    /**
     * Returns true if the repective blockCapacity's and bit maps 
     * are equal, false otherwise.
     */
    bool equals(FreeSpaceMap other);

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
     * Allocates `count` bits of byte with index `index`, starting at `startPos`
     */
    void allocateBitsInByte(uint32_t index, uint32_t startPos, uint32_t count);

    /**
     * Frees `count` bits of byte with index `index`, starting at `startPos`
     */
    void freeBitsInByte(uint32_t index, uint32_t startPos, uint32_t count);
};

/**
 * Test suite for FreeSpaceMap
 */
namespace FreeSpaceMapTests
{
    void testFreeNBlocks();
    void testAllocateNBlocks();
    void testAllocateThenFree();
    void runAll();
};
