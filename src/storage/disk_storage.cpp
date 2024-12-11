#include <string>

#include "disk_storage.hpp"

#include "utils.hpp"
#include "block.hpp"
#include "crypto.hpp"
#include "free_space.hpp"

#include "test_utils.hpp"

namespace fs = std::filesystem;

////////////////////////////////////////////
// Header methods
////////////////////////////////////////////

Header::Header()
{
}

Header::Header(
    uint32_t magicNumber, 
    uint32_t batOffset, 
    uint32_t batSize,
    uint32_t diskBlockSize, 
    uint32_t maxDataSize,
    uint32_t blockStoreOffset

)
    : magicNumber(magicNumber), 
        batOffset(batOffset), 
        batSize(batSize),
        diskBlockSize(diskBlockSize), 
        maxDataSize(maxDataSize),
        blockStoreOffset(blockStoreOffset)
        
{
}

bool Header::equals(Header& other) {
    return (
        magicNumber == other.magicNumber &&
        batOffset == other.batOffset &&
        batSize == other.batSize &&
        diskBlockSize == other.diskBlockSize &&
        maxDataSize == other.maxDataSize &&
        blockStoreOffset == other.blockStoreOffset
    );
}

std::string Header::toString() {
    std::stringstream ss;
    ss << "\nHeader:\n"
        << "  Magic Number: " << magicNumber << "\n"
        << "  BAT Offset: " << batOffset << "\n"
        << "  BAT Size: " << batSize << "\n"
        << "  Block Size: " << diskBlockSize << "\n"
        << "  Max Data Size: " << maxDataSize << "\n"
        << "  Block store offset: " << blockStoreOffset;
        
    return ss.str();
}

////////////////////////////////////////////
// BATEntry methods
////////////////////////////////////////////

BATEntry::BATEntry()
    : keyHash(0),
        startingDiskBlockNum(0),
        numBytes(0)
{
}

BATEntry::BATEntry(uint32_t keyHash, 
    uint32_t startingDiskBlockNum,
    uint32_t numBytes
)
    : keyHash(keyHash),
        startingDiskBlockNum(startingDiskBlockNum),
        numBytes(numBytes)
{
}

bool BATEntry::equals(BATEntry &other)
{
    return (
        keyHash == other.keyHash &&
        startingDiskBlockNum == other.startingDiskBlockNum &&
        numBytes == other.numBytes
    );
}

std::string BATEntry::toString() 
{
    std::ostringstream oss;

    oss << "    keyHash: 0x" << std::hex << std::setw(8) << std::setfill('0') << keyHash << "\n"
        << "    startingDiskBlockNum: " << std::dec << startingDiskBlockNum << "\n"
        << "    numBytes: " << numBytes << "\n";
    return oss.str();
}

////////////////////////////////////////////
// BAT methods
////////////////////////////////////////////

BAT::BAT()
    : numEntries(0)
{
}

BAT::BAT(uint32_t numEntries)
    : numEntries(numEntries),
        table(std::vector<BATEntry>(numEntries))
{
}

/**
 * Finds and returns an iterator to `key`'s corresponding BAT entry.
 */
std::optional<std::vector<BATEntry>::iterator> BAT::findBATEntry(uint32_t keyHash)
{
    for (auto it = table.begin(); it != table.end(); ++it)
    {
        if (it->keyHash == keyHash)
            return it;
    }
    return std::nullopt;
}

bool BAT::equals(BAT other)
{
    if (numEntries != other.numEntries)
        return false;
    
    for (int i = 0; i < numEntries; i++)
    {
        if (!table[i].equals(other.table[i]))
            return false;
    }
    return true;
}

std::string BAT::toString() 
{
    std::ostringstream oss;

    oss << "\nBAT:\n"
        << "  Num. entries: " << numEntries << "\n"
        << "  Table:\n";
    
    for (BATEntry be : table) {
        oss << be.toString() << "\n";
    }
    
    return oss.str();
}

////////////////////////////////////////////
// DiskStorage - public methods
////////////////////////////////////////////

/**
 * Default constructor
 */
DiskStorage::DiskStorage(
    std::string storeDirPath,
    std::string storeFileName,
    uint32_t diskBlockSize,
    uint32_t maxDataSize,
    bool removeExistingStore
)
    : storeDirPath(fs::path(storeDirPath)),
      storeFileName(storeFileName),
      removeExistingStore(removeExistingStore)
{
    this->storeFilePath = this->storeDirPath / this->storeFileName;

    if (removeExistingStore)
        FileSystemUtils::removeDirectory(this->storeDirPath);

    // initialise from existing store file
    if (!removeExistingStore && fs::exists(this->storeFilePath))
    {
        std::cout << "Reading from existing store file: " << this->storeFilePath << std::endl;

        readHeader();
        readBAT();
        freeSpaceMap.initialise(getNumDiskBlocks(maxDataSize));
        populateFreeSpaceMapFromFile();
    }

    // create new store file
    else 
    {
        createStoreFile();
        initialiseHeader(diskBlockSize, maxDataSize);
        writeHeader();
        freeSpaceMap.initialise(getNumDiskBlocks(maxDataSize));
    }
}

DiskStorage::~DiskStorage()
{
}

/**
 * Retreive and return all blocks of the given key.
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
std::vector<Block> DiskStorage::readBlocks(std::string key, std::vector<unsigned char> &readBuffer)
{
    // find BAT entry of `key`
    auto entry = this->bat.findBATEntry(Crypto::sha256_32(key));
    if (entry == std::nullopt)
        throw std::runtime_error("readBlocks() - no BAT entry found for given key: " + key);

    auto batEntry = *entry;

    /**
     * Read all block data into a single buffer.
     */
    uint32_t offset = getDiskBlockOffset(batEntry->startingDiskBlockNum);
    uint32_t numBytes = batEntry->numBytes;

    readBuffer.resize(numBytes);

    this->storeFile.open(storeFilePath, std::fstream::in | std::fstream::out);
    if (this->storeFile.is_open())
    {
        this->storeFile.seekg(offset);
        this->storeFile.read(reinterpret_cast<char*>(readBuffer.data()), numBytes);

        if (this->storeFile.fail() || this->storeFile.bad())
            throw std::runtime_error("readBlocks() - bad read of cumulative block data from disk");

        this->storeFile.close();
    }
    else
        throw std::runtime_error("failed to open file for reading!");
    
    if (readBuffer.size() != numBytes)
        throw std::runtime_error("readBlocks() - read buffer size != on-disk size");
    
    /**
     * Populate Block objects from the buffer.
     */
    std::vector<Block> blocks;

    uint32_t numBlocks = getNumDiskBlocks(numBytes);
    auto bufferIterator = readBuffer.begin();

    for (uint32_t i = 0; i < numBlocks; i++) 
    {
        uint32_t dataSize = std::min(
            this->header.diskBlockSize,
            static_cast<uint32_t>(std::distance(bufferIterator, readBuffer.end()))
        );

        Block block;

        block.key = key;
        block.blockNum = i;
        block.dataSize = dataSize;
        block.dataStart = bufferIterator;
        block.dataEnd = bufferIterator + dataSize;

        blocks.push_back(std::move(block));

        bufferIterator += dataSize;
    }
        
    return blocks;
}

/**
 * Write the given blocks for the given key.
 * 
 * Throws:
 *      runtime_error() - on any error during the writing process
 * 
 * NOTE: 
 * 
 * If `key` already exists, we overwrite its
 * existing blocks and BAT entry.
 */
void DiskStorage::writeBlocks(std::string key, std::vector<Block> dataBlocks)
{
    if (dataBlocks.size() == 0)
        throw std::runtime_error("writeBlocks() - no data blocks given");

    auto entry = this->bat.findBATEntry(Crypto::sha256_32(key));

    /**
     * If an entry already exists for the key, free its blocks
     * in the freeSpaceMap.
     * 
     * NOTE: if the new block allocation fails, we use `freedBlocks`
     *       to restore the old block allocation
     */
    std::pair<uint32_t, uint32_t> freedBlocks;
    if (entry != std::nullopt)
    {
        auto existingBatEntry = *entry;

        uint32_t oldStartingDiskBlockNum = existingBatEntry->startingDiskBlockNum;
        uint32_t oldN = getNumDiskBlocks(existingBatEntry->numBytes);
        freedBlocks = {oldStartingDiskBlockNum, oldN};

        freeSpaceMap.freeNBlocks(oldStartingDiskBlockNum, oldN);
    }

    // helper lambda to restore any freed blocks
    auto restoreFreedBlocks = [&]() {
    if (entry != std::nullopt)
        this->freeSpaceMap.allocateNBlocks(freedBlocks.first, freedBlocks.second);
    };

    /**
     * Find a contiguous section of N free disk blocks and retreive
     * the starting block number.
     */
    uint32_t N = dataBlocks.size();
    auto alloc = freeSpaceMap.findNFreeBlocks(N);
    if (alloc == std::nullopt)
        throw std::runtime_error("writeBlocks() - no contiguous section of " + std::to_string(N) + " blocks found");

    /**
     * Copy all block data into a single buffer (which we later write out to disk).
     * 
     * NOTE: we do this because an in-memory copy and a single disk I/O is 
     *       faster than N disk I/O's (espeically for large block sizes)
     */
    std::vector<unsigned char> buffer;
    uint32_t numBytes = 0;
    unsigned char blockNum[sizeof(uint32_t)];

    for (auto &dataBlock : dataBlocks)
    {
        // // write block number
        // std::memcpy(blockNum, &dataBlock.blockNum, sizeof(dataBlock.blockNum));
        // buffer.insert(buffer.end(), blockNum, blockNum + sizeof(dataBlock.blockNum));
        // numBytes += sizeof(dataBlock.blockNum);

        // write data
        buffer.insert(buffer.end(), dataBlock.dataStart, dataBlock.dataEnd);
        numBytes += dataBlock.dataSize;
    }

    if (buffer.size() != numBytes)
    {
        restoreFreedBlocks();
        throw std::runtime_error("writeBlocks() - bad copy of data blocks to output buffer");
    }
        
    // write buffer out to disk
    uint32_t startingDiskBlockNum = *alloc;
    uint32_t offset = getDiskBlockOffset(startingDiskBlockNum);

    this->storeFile.open(storeFilePath, std::fstream::in | std::fstream::out);
    if (this->storeFile.is_open())
    {
        this->storeFile.seekp(offset);
        this->storeFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

        if (this->storeFile.fail() || this->storeFile.bad())
        {
            restoreFreedBlocks();
            throw std::runtime_error("writeBlocks() - bad write of cumulative block data to disk");
        }
            
        this->storeFile.close();
    }
    else
    {
        restoreFreedBlocks();
        throw std::runtime_error("failed to open file for writing!");
    }

    // allocate new blocks
    freeSpaceMap.allocateNBlocks(startingDiskBlockNum, N);

    // update existing BAT entry
    if (entry != std::nullopt)
    {
        auto existingBatEntry = *entry;

        // update entry fields
        existingBatEntry->startingDiskBlockNum = startingDiskBlockNum;
        existingBatEntry->numBytes = numBytes;

        /**
         * NOTE: at this point, the existing blocks have already been freed
         */
    } 

    // create and insert new BAT entry
    else 
    {
        // allocate new blocks
        freeSpaceMap.allocateNBlocks(startingDiskBlockNum, N);

        // insert new entry
        BATEntry batEntry(Crypto::sha256_32(key), startingDiskBlockNum, numBytes);
        bat.table.push_back(std::move(batEntry));
        bat.numEntries++;
    }

    // write out updated BAT
    writeBAT();
}

/**
 * Deletes the BAT entry and frees the blocks of the given `key`.
 * 
 * Throws:
 *      runtime_error - on any error during the deleting process
 */
void DiskStorage::deleteBlocks(std::string key)
{
    // find `key`s BAT entry
    auto entry = this->bat.findBATEntry(Crypto::sha256_32(key));
    if (entry == std::nullopt)
        throw std::runtime_error("deleteBlocks() - no BAT entry exists for key: " + key);
    
    auto batEntry = *entry;

    /**
     * Free block bits in free space map.
     * 
     * NOTE: 
     * 
     * We do not override actual block data. Provided a block is considered 'free',
     * we can freely (pardon the pun) write over that block in the future.
     */
    uint32_t startingDiskBlockNum = batEntry->startingDiskBlockNum;
    uint32_t N = getNumDiskBlocks(batEntry->numBytes);

    this->freeSpaceMap.freeNBlocks(startingDiskBlockNum, N);

    // remove bat entry
    this->bat.numEntries--;
    this->bat.table.erase(batEntry);
}

/**
 * Returns total size (in bytes) of the store file.
 */
uint32_t DiskStorage::getTotalFileSize()
{
    return sizeof(this->header) + this->header.batSize + this->header.maxDataSize;
}

/**
 * Returns offset of disk block `diskBlockNum`.
 */
uint32_t DiskStorage::getDiskBlockOffset(uint32_t diskBlockNum)
{
    return this->header.blockStoreOffset + (this->header.diskBlockSize * diskBlockNum);
}

/**
 * Returns number of disk blocks `numBytes` bytes takes up.
 */
uint32_t DiskStorage::getNumDiskBlocks(uint32_t numBytes)
{
    return MathUtils::ceilDiv(numBytes, this->header.diskBlockSize);
}

////////////////////////////////////////////
// DiskStorage - private methods
////////////////////////////////////////////

/**
 * Creates a new store file in a new store directory.
 */
void DiskStorage::createStoreFile()
{
    // expand '~' to the home directory if present
    if (this->storeDirPath.string()[0] == '~')
    {
        const char* homeDir = std::getenv("HOME");
        this->storeDirPath = fs::path(homeDir) / this->storeDirPath.string().substr(2);
    }

    // create directory
    fs::create_directory(this->storeDirPath);
    std::cout << "Created store directory: " << this->storeDirPath << std::endl;

    // create file
    this->storeFile = std::fstream(this->storeFilePath, 
        std::fstream::in | std::fstream::out | std::fstream::trunc | std::fstream::binary);

    if (!this->storeFile.is_open())
        throw std::runtime_error("couldn't create store file");

    std::cout << "Created store file: " << this->storeFilePath << std::endl;

    // write to max byte
    uint32_t totalFileSize = this->getTotalFileSize();
    this->storeFile.seekp(totalFileSize - 1);
    this->storeFile.put(0);
    this->storeFile.flush();
    this->storeFile.close();
}

/**
 * Initialises the file header.
 */
void DiskStorage::initialiseHeader(uint32_t diskBlockSize, uint32_t maxDataSize)
{
    uint32_t batOffset = sizeof(Header);
    uint32_t numBlocks = MathUtils::ceilDiv(maxDataSize, diskBlockSize);
    uint32_t batSize = sizeof(uint32_t) + (numBlocks * sizeof(BATEntry));
    uint32_t blockStoreOffset = sizeof(Header) + batSize;

    this->header = Header(
        this->magicNumber, 
        batOffset, 
        batSize,
        diskBlockSize, 
        maxDataSize, 
        blockStoreOffset
    );
}

/**
 * Reads header from file and updates local copy (this->header).
 */
void DiskStorage::readHeader()
{
    this->storeFile.open(storeFilePath, std::fstream::in | std::fstream::out);

    if (this->storeFile.is_open()) {
        this->storeFile.seekg(0);
        this->storeFile.read(reinterpret_cast<char*>(&this->header), sizeof(this->header));
        if (!headerValid())
        {
            std::cout << "Error reading header" << std::endl;
        }
        this->storeFile.close();
    } else {
        std::cerr << "Failed to open file for reading header!" << std::endl;
    }
}

/**
 * Writes the local copy of the header (this->header) out to disk.
 */
void DiskStorage::writeHeader()
{
    this->storeFile.open(this->storeFilePath, std::fstream::in | std::fstream::out);

    if (this->storeFile.is_open()) {
        this->storeFile.seekp(0);
        this->storeFile.write(reinterpret_cast<char*>(&this->header), sizeof(this->header));
        this->storeFile.flush();
        this->storeFile.close();
    } else {
        std::cerr << "Failed to open file for writing header!" << std::endl;
    }
}

/**
 * Reads BAT from file and updates local copy (this->BAT).
 */
void DiskStorage::readBAT()
{
    this->storeFile.open(this->storeFilePath, std::fstream::in | std::fstream::out);

    if (this->storeFile.is_open()) {

        this->storeFile.seekg(sizeof(this->header));

        this->storeFile.read(reinterpret_cast<char*>(&this->bat.numEntries), sizeof(this->bat.numEntries));
        for (int i = 0; i < this->bat.numEntries; i++)
        {
            BATEntry be;
            this->storeFile.read(reinterpret_cast<char*>(&be), sizeof(be));
            if (i < this->bat.table.size())
                this->bat.table[i] = be;
            else
                this->bat.table.insert(this->bat.table.begin() + i, be);
        }
        this->storeFile.close();
    } else {
        std::cerr << "Failed to open file for reading BAT!" << std::endl;
    }
}

/**
 * Writes the local copy of the BAT (this->BAT) out to disk.
 */
void DiskStorage::writeBAT()
{
    this->storeFile.open(this->storeFilePath, std::fstream::in | std::fstream::out);

    if (this->storeFile.is_open()) {

        this->storeFile.seekp(sizeof(this->header));
        this->storeFile.write(reinterpret_cast<char*>(&this->bat.numEntries), sizeof(this->bat.numEntries));
        for (auto be : this->bat.table)
        {
            this->storeFile.write(reinterpret_cast<char*>(&be), sizeof(be));
        }
        this->storeFile.flush();
        this->storeFile.close();
    } else {
        std::cerr << "Failed to open file for writing BAT!" << std::endl;
    }
}

void DiskStorage::populateFreeSpaceMapFromFile()
{
    // ensure the free space map is already allocated to correct size
    ASSERT_THAT(this->freeSpaceMap.blockCapacity == getNumDiskBlocks(this->header.maxDataSize));

    /**
     * Allocate all blocks for each entry
     */
    uint32_t numBatEntries = this->bat.numEntries;
    for (int i = 0; i < numBatEntries; i++)
    {
        BATEntry entry = this->bat.table[i];
        uint32_t startingBlockNum = entry.startingDiskBlockNum;
        uint32_t numBlocks = getNumDiskBlocks(entry.numBytes);

        this->freeSpaceMap.allocateNBlocks(startingBlockNum, numBlocks);
    }
}

/**
 * Returns true if the local copy of the header is valid, false otherwise.
 */
bool DiskStorage::headerValid()
{
    return this->header.magicNumber == this->magicNumber;
}

/**
 * Reads `N` raw disk blocks into a buffer, starting at block `startingBlockNum`.
 * 
 * NOTE: used for debugging purposes mostly; such 
 *       buffers can be printed nicely using
 *       PrintUtils::printVector() (see utils.hpp)
 */
std::vector<unsigned char> DiskStorage::readRawDiskBlocks(uint32_t startingDiskBlockNum, uint32_t N)
{
    uint32_t numBytes = N * this->header.diskBlockSize;
    uint32_t offset = getDiskBlockOffset(startingDiskBlockNum);

    std::vector<unsigned char> buffer(numBytes);

    this->storeFile.open(storeFilePath, std::fstream::in | std::fstream::out);
    if (this->storeFile.is_open())
    {
        this->storeFile.seekg(offset);
        this->storeFile.read(reinterpret_cast<char*>(buffer.data()), numBytes);
        this->storeFile.close();
    }
    else
        throw std::runtime_error("failed to open file for reading!");
    
    return buffer;
}

////////////////////////////////////////////
// DiskStorage tests
////////////////////////////////////////////
namespace DiskStorageTests
{
    void setup()
    {
        // remove existing store of a previous test
        FileSystemUtils::removeDirectory(fs::path("rackkey"));
    }

    void teardown()
    {
        // remove store created during current test
        FileSystemUtils::removeDirectory(fs::path("rackkey"));
    }

    void testCanWriteAndReadNewHeaderAndBat()
    {
        setup();

        // implictly creates and writes header
        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);
        std::cout << ds.header.toString() << std::endl;

        // write some blocks
        std::string key = "archive.zip";
        uint32_t N = 2;
        uint32_t numBytes = N * blockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        key = "video.mp4";
        ds.writeBlocks(key, writeBlocks);

        ASSERT_THAT(ds.bat.numEntries == 2);

        Header oldHeader = ds.header;
        BAT oldBat = ds.bat;

        // instantiate new object so don't have header or BAT cached (i.e. must read from disk)
        DiskStorage newDs = DiskStorage("rackkey", "store");

        Header newHeader = newDs.header;
        BAT newBat = ds.bat;

        ASSERT_THAT(oldHeader.equals(newHeader));
        ASSERT_THAT(oldBat.equals(newBat));

        teardown();
    }

    /**
     * Tests that we can write a single key's blocks to disk and read them back out.
     */
    void testCanWriteAndReadOneKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        std::string key = "archive.zip";
        uint32_t N = 2;
        uint32_t numBytes = N * blockSize + 10;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        // write all blocks
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        // instantiate new object so don't have header, bat or file stream cached (i.e. must read from disk)
        DiskStorage newDs = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        // read blocks
        std::vector<unsigned char> readBuffer;
        std::vector<Block> readBlocks = newDs.readBlocks(key, readBuffer);

        // ensure what we wrote is what we read
        if (writeBlocks.size() != readBlocks.size())
        {
            throw std::runtime_error(
                ("write and read block lists not same size: " +
                  std::to_string(writeBlocks.size()) +
                  std::to_string(readBlocks.size()))
            );
        }
        for (uint32_t i = 0; i < writeBlocks.size(); i++)
            ASSERT_THAT(writeBlocks[i].equals(readBlocks[i]));
        
        teardown();
    }

    /**
     * Tests that we can write multiple keys' blocks to disk and read them back out.
     */
    void testCanWriteAndReadMultipleKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        std::vector<std::string> keys;
        std::vector<std::vector<Block>> writeBlocksList;
        std::vector<std::vector<std::vector<unsigned char>>> writeDataBuffersList;

        uint32_t M = 5;

        // generate data for M keys
        for (uint32_t i = 0; i < M; i++) 
        {
            std::string key = "key_" + std::to_string(i);
            keys.push_back(key);

            uint32_t N = i + 1; // Vary the number of blocks per key
            uint32_t numBytes = N * blockSize + (i % blockSize);
            std::vector<std::vector<unsigned char>> writeDataBuffers;

            // Generate and write blocks
            std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
            ds.writeBlocks(key, writeBlocks);

            // Store for later validation
            writeBlocksList.push_back(writeBlocks);
            writeDataBuffersList.push_back(std::move(writeDataBuffers));
        }

        // Instantiate new object to ensure no cached data (read from disk)
        DiskStorage newDs = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        // Read and validate data for each key
        for (uint32_t i = 0; i < M; i++) 
        {
            std::cout << "/////////////////////////////////////////////////" << std::endl;
            std::cout << "// Key " << i << std::endl;
            std::cout << "/////////////////////////////////////////////////" << std::endl << std::endl;

            std::string& key = keys[i];
            std::vector<Block>& expectedBlocks = writeBlocksList[i];
            std::vector<unsigned char> totalWriteBuffer = VectorUtils::flatten(writeDataBuffersList[i]);

            std::cout << "Expected blocks: " << std::endl << std::endl;
            for (auto block : expectedBlocks)
                std::cout << block.toString(true) << std::endl;

            // Read blocks
            std::vector<unsigned char> readBuffer;
            std::vector<Block> readBlocks = newDs.readBlocks(key, readBuffer);

            std::cout << "Read blocks: " << std::endl << std::endl;
            for (auto block : readBlocks)
                std::cout << block.toString(true) << std::endl;

            // Validate size and content
            ASSERT_THAT(expectedBlocks.size() == readBlocks.size());
            for (uint32_t j = 0; j < expectedBlocks.size(); j++) 
            {
                ASSERT_THAT(expectedBlocks[j].equals(readBlocks[j]));
            }

            ASSERT_THAT(totalWriteBuffer == readBuffer);
        }

        std::cout << newDs.header.toString() << std::endl;
        std::cout << newDs.bat.toString() << std::endl;

        teardown();
    }

    void testCanDeleteOneKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        // write some blocks
        std::string key = "archive.zip";
        uint32_t N = 2;
        uint32_t extraBytes = 10;
        uint32_t numBytes = N * blockSize + extraBytes;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);
        
        // should have written (N + 1) blocks, starting at blockNum = 0
        for (int i = 0; i < N + 1; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));
        ASSERT_THAT(ds.bat.numEntries == 1);

        // delete blocks
        ds.deleteBlocks(key);

        // first (N + 1) blocks should now be free
        for (int i = 0; i < N + 1; i++)
            ASSERT_THAT(!ds.freeSpaceMap.isMapped(i));

        ASSERT_THAT(ds.bat.numEntries == 0 && ds.bat.table.size() == 0);

        teardown();
    }

    void testCanBuildUpFreeSpaceMapFromExistingFile()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 10);

        // write some blocks
        std::string key = "archive.zip";
        uint32_t N = 2;
        uint32_t numBytes = N * blockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        // write some more blocks, for another key
        std::string newKey = "video.mp4";
        uint32_t newN = 3;
        uint32_t newNumBytes = newN * blockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(newKey, newN, blockSize, newNumBytes, writeDataBuffers);
        ds.writeBlocks(newKey, writeBlocks);

        std::cout << ds.freeSpaceMap.toString() << std::endl;

        // create a new DiskStorage object - to force an initialisation from file
        DiskStorage newDs = DiskStorage("rackkey", "store", blockSize, 1u << 10);

        std::cout << newDs.freeSpaceMap.toString() << std::endl;

        ASSERT_THAT(newDs.freeSpaceMap.blockCapacity == newDs.getNumDiskBlocks(newDs.header.maxDataSize));
        for (int i = 0; i < N + newN; i++)
            ASSERT_THAT(newDs.freeSpaceMap.isMapped(i));
        for (int i = N + newN; i < newDs.freeSpaceMap.blockCapacity; i++)
            ASSERT_THAT(!newDs.freeSpaceMap.isMapped(i));
        
        teardown();
    }

    void testCanOverwriteExistingKey()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        std::string key = "archive.zip";
        uint32_t N = 5;
        uint32_t numBytes = N * blockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        // write N blocks
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key));

        // ensure blocks were correctly written
        ASSERT_THAT((*entry)->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));
        
        // overwrite with M < N blocks
        uint32_t M = N - 2;
        numBytes = M * blockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(key, M, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        entry = ds.bat.findBATEntry(Crypto::sha256_32(key));

        // ensure didn't write a new entry (i.e. ensure we overwrote the old entry)
        ASSERT_THAT(ds.bat.numEntries == 1);
        ASSERT_THAT((*entry)->keyHash == Crypto::sha256_32(key));

        // ensure new blocks were correctly written
        ASSERT_THAT((*entry)->numBytes == numBytes);
        for (int i = 0; i < M; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));
        
        // ensure old blocks were de-allocated
        for (int i = M; i < N; i++)
            ASSERT_THAT(!ds.freeSpaceMap.isMapped(i));
        
        teardown();
    }

    void testFragmentedWrite()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", blockSize, 1u << 30);

        std::string key1 = "archive.zip";
        uint32_t N = 3;
        uint32_t numBytes = N * blockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        // write N blocks for first key
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key1, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key1, writeBlocks);

        // write M blocks for second key (doesn't really matter how many)
        std::string key2 = "video.mp4";
        uint32_t M = 5;
        numBytes = M * blockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(key2, M, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key2, writeBlocks);

        // delete first key (i.e. free first N blocks)
        ds.deleteBlocks(key1);

        // write (N + 1) blocks for third key
        // i.e. should skip first free N blocks, and write starting after key2's blocks
        std::string key3 = "shakespeare.txt";
        numBytes = (N + 1) * blockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(key3, (N + 1), blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key3, writeBlocks);

        // ensure `key1`s blocks are unmapped
        for (int i = 0; i < N; i++)
            ASSERT_THAT(!ds.freeSpaceMap.isMapped(i));

        // ensure `key2`s and `key3`s blocks are mapped
        for (int i = N; i < N + M + (N + 1); i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));

        // ensure `key3`s blocks start at block N + M
        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key3));
        ASSERT_THAT((*entry)->startingDiskBlockNum == N + M);

        teardown();
    }

    void testMaxBlocksReached()
    {
        setup();

        DiskStorage ds = DiskStorage("rackkey", "store", 4096, 1u << 20);

        /**
         * 1MB max data size (2^20), 4KB block size (2^12) -> 256 (2^8) blocks
         */
        uint32_t maxNumBlocks = ds.getNumDiskBlocks(ds.header.maxDataSize);
        ASSERT_THAT(maxNumBlocks == 256);

        // should successfully write N blocks
        std::string key = "archive.zip";
        uint32_t N = 230;
        uint32_t numBytes = N * ds.header.diskBlockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, ds.header.diskBlockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key));
        auto batEntry = *entry;

        // ensure first write was valid
        ASSERT_THAT(batEntry->startingDiskBlockNum == 0);
        ASSERT_THAT(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));

        // should fail to write 1 more block than is available
        std::string newKey = "video.mp4";
        uint32_t newN = (maxNumBlocks - N) + 1;
        uint32_t newNumBytes = newN * ds.header.diskBlockSize;
        writeDataBuffers.clear();

        writeBlocks = BlockUtils::generateNRandom(newKey, newN, ds.header.diskBlockSize, newNumBytes, writeDataBuffers);
        try 
        {
            ds.writeBlocks(newKey, writeBlocks);
            
            // shouldn't reach this point - investigate
            std::cout << ds.bat.toString() << std::endl;
            std::cout << ds.freeSpaceMap.toString() << std::endl;
            return;
        }
        catch (std::runtime_error& e)
        {
            std::cout << e.what() << std::endl;
        }

        // ensure disk state has been maintained
        ASSERT_THAT(batEntry->startingDiskBlockNum == 0);
        ASSERT_THAT(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));

        teardown();
    }

    /**
     * Testing that, on a failed write, we restore the
     * old, valid disk state.
     * 
     * NOTE: 
     * 
     * Advantage of our approach is that the block data 
     * write occurs in one operation, so the only thing we 
     * have to restore are the previously allocated blocks
     * we greedily free'd.
     */
    void testRestoreDiskStateOnFailedWrite()
    {
        setup();

        DiskStorage ds = DiskStorage("rackkey", "store", 4096, 1u << 20);

        // should successfully write N blocks
        std::string key = "archive.zip";
        uint32_t N = 10;
        uint32_t numBytes = N * ds.header.diskBlockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, ds.header.diskBlockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        // ensure first write was valid
        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key));
        auto batEntry = *entry;
        ASSERT_THAT(batEntry->startingDiskBlockNum == 0);
        ASSERT_THAT(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));

        // construct an intentionally broken Block object
        uint32_t newN = 1;
        uint32_t newNumBytes = N * ds.header.diskBlockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(key, newN, ds.header.diskBlockSize, newNumBytes, writeDataBuffers);

        /**
         * Setting dataSize to 0 when underlying data is non-zero size
         * will cause writeBlocks() to cancel the write.
         */
        writeBlocks[0].dataSize = 0;

        try
        {
            ds.writeBlocks(key, writeBlocks);

            // shouldn't reach this point
            return;
        }
        catch (std::runtime_error e)
        {
            std::cout << e.what() << std::endl;
        }

        // ensure first write is intact
        entry = ds.bat.findBATEntry(Crypto::sha256_32(key));
        batEntry = *entry;
        ASSERT_THAT(batEntry->startingDiskBlockNum == 0);
        ASSERT_THAT(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            ASSERT_THAT(ds.freeSpaceMap.isMapped(i));

        teardown();
    }

    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "DiskStorageTests" << std::endl;
        std::cerr << "###################################" << std::endl;

        std::vector<std::pair<std::string, std::function<void()>>> tests = {
            TEST(testCanWriteAndReadNewHeaderAndBat),
            TEST(testCanWriteAndReadOneKeysBlocks),
            TEST(testCanWriteAndReadMultipleKeysBlocks),
            TEST(testCanDeleteOneKeysBlocks),
            TEST(testCanBuildUpFreeSpaceMapFromExistingFile),
            TEST(testCanOverwriteExistingKey),
            TEST(testFragmentedWrite),
            TEST(testMaxBlocksReached),
            TEST(testRestoreDiskStateOnFailedWrite)
        };

        for (auto &[name, func] : tests)
        {
            TestUtils::runTest(name, func);
        }

        std::cerr << std::endl;
    }
}

