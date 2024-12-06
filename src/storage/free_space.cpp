#include <string>

#include "free_space.hpp"
#include "utils.hpp"
#include "block.hpp"
#include "crypto.hpp"

////////////////////////////////////////////
// FreeSpaceMap: public methods
////////////////////////////////////////////

/**
 * Default constructor
 */
FreeSpaceMap::FreeSpaceMap(uint32_t blockCapacity)
    : blockCapacity(blockCapacity)
{
    int numEntries = ceil(blockCapacity / 8);
    this->bitMap = std::vector<uint8_t>(numEntries, 0);
}

/**
 * TODO:
 * 
 * Finds `N` contiguous free blocks and allocates them.
 * 
 * Returns the starting block number.
 */
std::optional<uint32_t> FreeSpaceMap::allocateNBlocks(uint32_t N)
{
    if (N < 1)
        return std::nullopt;

    // find contiguous section of `N` free blocks
    auto alloc = findNFreeBlocks(N);
    if (alloc == std::nullopt) 
        return std::nullopt;

    uint32_t startBlockNum = *alloc;
    std::cout << "Starting block: " << startBlockNum << std::endl;

    uint32_t index = startBlockNum / 8; // byte index
    uint32_t pos = startBlockNum % 8;   // position in byte

    // allocate any un-aligned blocks at the beginning
    if (pos != 0)
    {
        uint32_t numBits = std::min(N, 8 - pos);
        allocateBitsInByte(index, pos, numBits);
        N -= numBits;
        index++;
    }

    // allocate aligned blocks
    while (N >= 8)
    {
        bitMap[index++] = 0xFF;
        N -= 8;
    }

    // allocate any un-aligned blocks at the end
    if (N > 0)
        allocateBitsInByte(index, 0, N);

    return startBlockNum;
}

/**
 * Frees `N` contiguous blocks starting at block number `startBlockNum`.
 */
void FreeSpaceMap::freeNBlocks(uint32_t startBlockNum, uint32_t N)
{
    if (N < 1)
        return;

    uint32_t index = startBlockNum / 8; // byte index
    uint32_t pos = startBlockNum % 8;   // position in byte

    // free any un-aligned blocks at the beginning
    if (pos != 0)
    {
        uint32_t numBits = std::min(N, 8 - pos);
        freeBitsInByte(index, pos, numBits);
        N -= numBits;
        index++;
    }

    // free aligned blocks
    while (N >= 8)
    {
        bitMap[index++] = 0;
        N -= 8;
    }

    // free any un-aligned blocks at the end
    if (N > 0)
    {
        freeBitsInByte(index, 0, N);
    }
}

/**
 * Returns string representation of the free space map.
 * 
 * NOTE:
 * 
 * By default, we only show mapped blocks.
 * Setting showUnMapped = true shows unmapped blocks.
 */
std::string FreeSpaceMap::toString(bool showUnMapped)
{
    std::ostringstream oss;
    oss << "\nFree space map" << std::endl;
    oss << "---" << std::endl;
    for (uint32_t i = 0; i < blockCapacity; i++)
    {
        bool mapped = isMapped(i);
        if (mapped || showUnMapped)
        {
            oss << "Block " << i << " : " << mapped << std::endl;
        }
    }
    oss << "---" << std::endl;
    return oss.str();  // Convert the stream to a string and return it
}

////////////////////////////////////////////
// FreeSpaceMap: private methods
////////////////////////////////////////////

/**
 * TODO:
 * 
 * Finds `N` contiguous free blocks and returns the 
 * starting block number.
 */
std::optional<uint32_t> FreeSpaceMap::findNFreeBlocks(uint32_t N)
{
    uint32_t index = 0;   // index of current entry in bitmap
    uint32_t pos = 0;     // bit position

    uint32_t currStartBlock = 0; 
    uint32_t contiguousCount = 0;

    while (index < this->bitMap.size())
    {
        uint8_t currEntry = bitMap[index];

        for (; pos < 8; pos++)
        {
            // block is free
            if ((currEntry & (1 << pos)) == 0)
            {
                if (contiguousCount == 0)
                {
                    // mark start of new contiguous section
                    currStartBlock = index * 8 + pos;
                }
                contiguousCount++;

                // found contiguous section
                if (contiguousCount == N)
                {
                    return currStartBlock;
                }
            }

            // block is allocated
            else
            {
                contiguousCount = 0;
            }
        }

        // move to next entry
        index++;
        pos = 0;
    }

    // no contiguous section found
    std::cout << "No contiguous section of " << N << " blocks available" << std::endl;
    return std::nullopt;
}

/**
 * Allocates `count` bits of byte with index `index`, starting at `startPos`
 */
void FreeSpaceMap::allocateBitsInByte(uint32_t index, uint32_t startPos, uint32_t count)
{
    uint32_t mask = ((1 << count) - 1) << startPos; // 1's in positions to be allocated
    bitMap[index] |= mask;
}

/**
 * Frees `count` bits of byte with index `index`, starting at `startPos`
 */
void FreeSpaceMap::freeBitsInByte(uint32_t index, uint32_t startPos, uint32_t count)
{
    uint32_t mask = ((1 << count) - 1) << startPos;  // 1's in positions to be free'd
    bitMap[index] &= ~mask; 
}

/**
 * Returns true if the given block is mapped, false if its free
 */
bool FreeSpaceMap::isMapped(uint32_t blockNum)
{
    int index = blockNum / 8;
    if (index >= bitMap.size())
        throw std::runtime_error("Block number not mapped: " + std::to_string(blockNum));

    int pos = blockNum % 8;
    return bitMap[index] >> pos & 0x01;
}

////////////////////////////////////////////
// FreeSpaceMap tests
////////////////////////////////////////////
namespace FreeSpaceMapTests
{
    void testFreeNBlocks()
    {
        int blockCapacity = 32;
        FreeSpaceMap fsm(blockCapacity);

        // allocate some blocks
        fsm.bitMap[0] = 0xFF; // 8 blocks
        fsm.bitMap[1] = 0xFF; // 8 blocks
        fsm.bitMap[2] = 0xFF; // 8 blocks
        fsm.bitMap[3] = 0x03; // 2 blocks

        std::cout << fsm.toString(true);

        // free `N` blocks starting at `startingBlockNum`
        uint32_t startingBlockNum = 14;
        uint32_t N = 12;
        fsm.freeNBlocks(startingBlockNum, N);

        std::cout << fsm.toString(true);
    }

    void testAllocateNBlocks()
    {
        int blockCapacity = 32;
        FreeSpaceMap fsm(blockCapacity);
        uint32_t N = 10;

        // allocate some troublesome blocks
        fsm.bitMap[0] = 0x7F; // 0111 1111
        fsm.bitMap[1] = 0x00; // 0000 0000
        fsm.bitMap[2] = 0x01; // 0000 0001
        fsm.bitMap[3] = 0x00; // 0000 0000
        
        fsm.allocateNBlocks(N);
        std::cout << fsm.toString(true);
    }

    void testAllocateThenFree()
    {
        int blockCapacity = 32;
        FreeSpaceMap fsm(blockCapacity);

        // pre-allocate some blocks
        fsm.bitMap[0] = 0x7F; // 0111 1111
        std::cout << fsm.toString(true);

        // allocate N blocks - should start at block 7
        uint32_t N = 10;
        fsm.allocateNBlocks(N);
        std::cout << fsm.toString(true);

        // free those N blocks
        uint32_t startBlockNum = 7;
        fsm.freeNBlocks(7, N);
        std::cout << fsm.toString(true);
    }
};

// /**
//  * Free space map used to find contiguous chunks of blocks on disk.
//  */
// class FreeSpaceMap
// {
// public:

//     /**
//      * Number of blocks bitmap keeps track of
//      */
//     uint32_t blockCapacity;

//     /**
//      * Bitmap data structure used
//      */
//     std::vector<uint8_t> bitMap;

//     /**
//      * Default constructor
//      */
//     FreeSpaceMap(uint32_t blockCapacity)
//         : blockCapacity(blockCapacity)
//     {
//         int numEntries = ceil(blockCapacity / 8);
//         this->bitMap = std::vector<uint8_t>(numEntries, 0);
//     }

//     /**
//      * TODO:
//      * 
//      * Finds `N` contiguous free blocks and allocates them.
//      * 
//      * Returns the starting block number.
//      */
//     std::optional<uint32_t> allocateNBlocks(uint32_t N)
//     {
//         if (N < 1)
//             return std::nullopt;

//         // find contiguous section of `N` free blocks
//         auto alloc = findNFreeBlocks(N);
//         if (alloc == std::nullopt) 
//             return std::nullopt;

//         uint32_t startBlockNum = *alloc;
//         std::cout << "Starting block: " << startBlockNum << std::endl;

//         uint32_t index = startBlockNum / 8; // byte index
//         uint32_t pos = startBlockNum % 8;   // position in byte

//         // allocate any un-aligned blocks at the beginning
//         if (pos != 0)
//         {
//             uint32_t numBits = std::min(N, 8 - pos);
//             allocateBitsInByte(index, pos, numBits);
//             N -= numBits;
//             index++;
//         }

//         // allocate aligned blocks
//         while (N >= 8)
//         {
//             bitMap[index++] = 0xFF;
//             N -= 8;
//         }

//         // allocate any un-aligned blocks at the end
//         if (N > 0)
//             allocateBitsInByte(index, 0, N);

//         return startBlockNum;
//     }

//     /**
//      * Frees `N` contiguous blocks starting at block number `startBlockNum`.
//      */
//     void freeNBlocks(uint32_t startBlockNum, uint32_t N)
//     {
//         if (N < 1)
//             return;

//         uint32_t index = startBlockNum / 8; // byte index
//         uint32_t pos = startBlockNum % 8;   // position in byte

//         // free any un-aligned blocks at the beginning
//         if (pos != 0)
//         {
//             uint32_t numBits = std::min(N, 8 - pos);
//             freeBitsInByte(index, pos, numBits);
//             N -= numBits;
//             index++;
//         }

//         // free aligned blocks
//         while (N >= 8)
//         {
//             bitMap[index++] = 0;
//             N -= 8;
//         }

//         // free any un-aligned blocks at the end
//         if (N > 0)
//         {
//             freeBitsInByte(index, 0, N);
//         }
//     }

//     /**
//      * Returns string representation of the free space map.
//      * 
//      * NOTE:
//      * 
//      * By default, we only show mapped blocks.
//      * Setting showUnMapped = true shows unmapped blocks.
//      */
//     std::string toString(bool showUnMapped = false)
//     {
//         std::ostringstream oss;
//         oss << "\nFree space map" << std::endl;
//         oss << "---" << std::endl;
//         for (uint32_t i = 0; i < blockCapacity; i++)
//         {
//             bool mapped = isMapped(i);
//             if (mapped || showUnMapped)
//             {
//                 oss << "Block " << i << " : " << mapped << std::endl;
//             }
//         }
//         oss << "---" << std::endl;
//         return oss.str();  // Convert the stream to a string and return it
//     }

// private:

//     /**
//      * TODO:
//      * 
//      * Finds `N` contiguous free blocks and returns the 
//      * starting block number.
//      */
//     std::optional<uint32_t> findNFreeBlocks(uint32_t N)
//     {
//         uint32_t index = 0;   // index of current entry in bitmap
//         uint32_t pos = 0;     // bit position

//         uint32_t currStartBlock = 0; 
//         uint32_t contiguousCount = 0;

//         while (index < this->bitMap.size())
//         {
//             uint8_t currEntry = bitMap[index];

//             for (; pos < 8; pos++)
//             {
//                 // block is free
//                 if ((currEntry & (1 << pos)) == 0)
//                 {
//                     if (contiguousCount == 0)
//                     {
//                         // mark start of new contiguous section
//                         currStartBlock = index * 8 + pos;
//                     }
//                     contiguousCount++;

//                     // found contiguous section
//                     if (contiguousCount == N)
//                     {
//                         return currStartBlock;
//                     }
//                 }

//                 // block is allocated
//                 else
//                 {
//                     contiguousCount = 0;
//                 }
//             }

//             // move to next entry
//             index++;
//             pos = 0;
//         }

//         // no contiguous section found
//         std::cout << "No contiguous section of " << N << " blocks available" << std::endl;
//         return std::nullopt;
//     }

//     /**
//      * Allocates `count` bits of byte with index `index`, starting at `startPos`
//      */
//     void allocateBitsInByte(uint32_t index, uint32_t startPos, uint32_t count)
//     {
//         uint32_t mask = ((1 << count) - 1) << startPos; // 1's in positions to be allocated
//         bitMap[index] |= mask;
//     }

//     /**
//      * Frees `count` bits of byte with index `index`, starting at `startPos`
//      */
//     void freeBitsInByte(uint32_t index, uint32_t startPos, uint32_t count)
//     {
//         uint32_t mask = ((1 << count) - 1) << startPos;  // 1's in positions to be free'd
//         bitMap[index] &= ~mask; 
//     }

//     /**
//      * Returns true if the given block is mapped, false if its free
//      */
//     bool isMapped(uint32_t blockNum)
//     {
//         int index = blockNum / 8;
//         if (index >= bitMap.size())
//             throw std::runtime_error("Block number not mapped: " + std::to_string(blockNum));

//         int pos = blockNum % 8;
//         return bitMap[index] >> pos & 0x01;
//     }
// };
