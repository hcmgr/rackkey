#pragma once

#include <string>

#include "utils.hpp"
#include "block.hpp"
#include "crypto.hpp"
#include "free_space.hpp"

#include "test_utils.hpp"

namespace fs = std::filesystem;

/**
 * Represents the header of our storage file.
 */
struct __attribute__((packed)) Header 
{
    uint32_t magicNumber;   
    uint32_t batOffset;    
    uint32_t batSize;       
    uint32_t diskBlockSize;    
    uint32_t maxDataSize;
    uint32_t blockStoreOffset;
    
    Header();

    Header(
        uint32_t magicNumber, 
        uint32_t batOffset, 
        uint32_t batSize,
        uint32_t diskBlockSize, 
        uint32_t maxDataSize,
        uint32_t blockStoreOffset
    );

    bool equals(Header& other);
    std::string toString();
};

/**
 * Represents an entry in the BAT.
 */
struct __attribute__((packed)) BATEntry
{
    uint32_t keyHash;
    uint32_t startingDiskBlockNum;
    uint32_t numBytes;

    BATEntry();

    BATEntry(uint32_t keyHash, 
        uint32_t startingDiskBlockNum,
        uint32_t numBytes
    );

    bool equals(BATEntry &other);
    std::string toString();
};

/**
 * Represents our block allocation table (BAT).
 */
struct BAT
{
    uint32_t numEntries;
    std::vector<BATEntry> table;

    BAT();

    BAT(uint32_t numEntries);

    /**
     * Finds and returns an iterator to `key`'s corresponding BAT entry.
     */
    std::optional<std::vector<BATEntry>::iterator> findBATEntry(uint32_t keyHash);

    bool equals(BAT other);
    std::string toString();
};

/**
 * Represents our storage nodes on-disk storage.
 */
class DiskStorage
{
public:

    /* File header */
    Header header;

    /* Block allocation table */
    BAT bat;

    /* Free space map for our block store */
    FreeSpaceMap freeSpaceMap;

    /**
     * Default constructor
     */
    DiskStorage(
        std::string storeDirPath = "rackkey",
        std::string storeFileName = "store",
        uint32_t diskBlockSize = 4096,
        uint32_t maxDataSize = 1u << 30,
        bool removeExistingStore = false
    );

    ~DiskStorage();

    /**
     * Retreive and return ALL blocks stored on this node for 
     * the given `key`.
     * 
     * Throws: 
     *      runtime_error() - on any error during the reading process
     * 
     * NOTE:
     *
     * `readBuffer` is the buffer we read the raw block data into 
     * and it may be emptied, as it is resized as needed.
     * 
     * 
     * We pass this in so that the data the block pointers reference
     * does not get de-allocated.
     */
    std::vector<Block> readBlocks(
        std::string key, 
        std::vector<uint32_t> blockNums,
        uint32_t dataBlockSize, 
        std::vector<unsigned char> &readBuffer);

    /**
     * Write the given list of blocks `dataBlocks` for the given `key`.
     * 
     * Throws:
     *      runtime_error() - on any error during the writing process
     * 
     * NOTE: 
     * 
     * If `key` already exists, we overwrite its
     * existing blocks and BAT entry.
     */
    void writeBlocks(std::string key, std::vector<Block> dataBlocks);

    /**
     * Deletes the BAT entry and frees the blocks of the given `key`.
     * 
     * Throws:
     *      runtime_error - on any error during the deleting process
     */
    void deleteBlocks(std::string key);

    /**
     * Reads `N` raw disk blocks into a buffer, starting at block `startingBlockNum`.
     * 
     * NOTE: used for debugging purposes mostly; such 
     *       buffers can be printed nicely using
     *       PrintUtils::printVector() (see utils.hpp)
     */
    std::vector<unsigned char> readRawDiskBlocks(uint32_t startingDiskBlockNum, uint32_t N);

    /**
     * Returns total size (in bytes) of the store file.
     */
    uint32_t getTotalFileSize();

    /**
     * Returns offset of disk block `diskBlockNum`.
     */
    uint32_t getDiskBlockOffset(uint32_t diskBlockNum);

    /**
     * Returns number of disk blocks `numDataBytes` bytes takes up.
     */
    uint32_t getNumDiskBlocks(uint32_t numDataBytes);

private:    

    const uint32_t magicNumber = 0xABABABAB;

    fs::path storeDirPath;
    std::string storeFileName;

    fs::path storeFilePath;
    std::fstream storeFile;

    bool removeExistingStore;

    /**
     * Creates a new store file in a new store directory.
     */
    void createStoreFile();

    /**
     * Initialises the file header.
     */
    void initialiseHeader(uint32_t diskBlockSize, uint32_t maxDataSize);

    /**
     * Reads header from file and updates local copy (this->header).
     */
    void readHeader();

    /**
     * Writes the local copy of the header (this->header) out to disk.
     */
    void writeHeader();

    /**
     * Reads BAT from file and updates local copy (this->BAT).
     */
    void readBAT();

    /**
     * Writes the local copy of the BAT (this->BAT) out to disk.
     */
    void writeBAT();

    /**
     * Builds up the free space map from an existing store file.
     */
    void populateFreeSpaceMapFromFile();

    /**
     * Returns true if the local copy of the header is valid, false otherwise.
     */
    bool headerValid();


};

////////////////////////////////////////////
// DiskStorage tests
////////////////////////////////////////////
namespace DiskStorageTests
{
    void setup();
    void teardown();

    void testCanWriteAndReadNewHeaderAndBat();
    void testCanWriteAndReadOneKeysBlocks();
    void testCanWriteAndReadMultipleKeysBlocks();
    void testCanDeleteOneKeysBlocks();
    void testCanBuildUpFreeSpaceMapFromExistingFile();
    void testCanOverwriteExistingKey();
    void testFragmentedWrite();
    void testMaxBlocksReached();
    void testRestoreDiskStateOnFailedWrite();

    void runAll();
}