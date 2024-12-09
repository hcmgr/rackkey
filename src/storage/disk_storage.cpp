#include <string>

#include "utils.hpp"
#include "block.hpp"
#include "crypto.hpp"
#include "free_space.hpp"

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
    
    Header()
    {
    }

    Header(
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

    bool equals(Header& other) {
        return (
            magicNumber == other.magicNumber &&
            batOffset == other.batOffset &&
            batSize == other.batSize &&
            diskBlockSize == other.diskBlockSize &&
            maxDataSize == other.maxDataSize &&
            blockStoreOffset == other.blockStoreOffset
        );
    }

    std::string toString() {
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
};

/**
 * Represents an entry in the BAT.
 */
struct __attribute__((packed)) BATEntry
{
    uint32_t keyHash;
    uint32_t startingDiskBlockNum;
    uint32_t numBytes;

    BATEntry()
        : keyHash(0),
          startingDiskBlockNum(0),
          numBytes(0)
    {
    }

    BATEntry(uint32_t keyHash, 
        uint32_t startingDiskBlockNum,
        uint32_t numBytes
    )
        : keyHash(keyHash),
          startingDiskBlockNum(startingDiskBlockNum),
          numBytes(numBytes)
    {
    }

    bool equals(BATEntry &other)
    {
        return (
            keyHash == other.keyHash &&
            startingDiskBlockNum == other.startingDiskBlockNum &&
            numBytes == other.numBytes
        );
    }

    std::string toString() 
    {
        std::ostringstream oss;

        oss << "    keyHash: 0x" << std::hex << std::setw(8) << std::setfill('0') << keyHash << "\n"
            << "    startingDiskBlockNum: " << std::dec << startingDiskBlockNum << "\n"
            << "    numBytes: " << numBytes << "\n";
        return oss.str();
    }
};

/**
 * Represents our block allocation table (BAT).
 */
struct BAT
{
    uint32_t numEntries;
    std::vector<BATEntry> table;

    BAT()
        : numEntries(0)
    {
    }

    BAT(uint32_t numEntries)
        : numEntries(numEntries),
          table(std::vector<BATEntry>(numEntries))
    {
    }

    /**
     * Finds and returns an iterator to `key`'s corresponding BAT entry.
     */
    std::optional<std::vector<BATEntry>::iterator> findBATEntry(uint32_t keyHash)
    {
        for (auto it = table.begin(); it != table.end(); ++it)
        {
            if (it->keyHash == keyHash)
                return it;
        }
        return std::nullopt;
    }

    bool equals(BAT other)
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

    std::string toString() 
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
};

class DiskStorage
{
public:
    uint32_t magicNumber;
    uint32_t diskBlockSize;
    uint32_t maxDataSize;

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
        uint32_t magicNumber = 0xABABABAB,
        uint32_t diskBlockSize = 4096,
        uint32_t maxDataSize = 1u << 30
    )
        : storeDirPath(fs::path(storeDirPath)),
          storeFileName(storeFileName),
          magicNumber(magicNumber),
          diskBlockSize(diskBlockSize),
          maxDataSize(maxDataSize),
          freeSpaceMap(getNumBlocks(maxDataSize))
    {
        this->storeFilePath = this->storeDirPath / this->storeFileName;

        // initialise from existing store file
        if (storeFileExists())
        {
            std::cout << "Reading from existing store file: " << this->storeFilePath << std::endl;

            readHeader();
            readBAT();
        }

        // create new store file
        else {
            initialiseHeader();
            createStoreFile();
            writeHeader();
        }
    }

    ~DiskStorage()
    {
    }

    uint32_t getNumBlocks(uint32_t numBytes)
    {
        return MathUtils::ceilDiv(numBytes, diskBlockSize);
    }

    uint32_t getTotalFileSize()
    {
        return sizeof(this->header) + this->header.batSize + this->header.maxDataSize;
    }

    /**
     * Reads header from file and updates local copy (this->header).
     */
    void readHeader()
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
    void writeHeader()
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
    void readBAT()
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
    void writeBAT()
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

    /**
     * Reads `N` raw disk blocks into a buffer, starting at block `startingBlockNum`.
     * 
     * NOTE: used for debugging purposes mostly; such 
     *       buffers can be printed nicely using
     *       PrintUtils::printVector() (see utils.hpp)
     */
    std::vector<unsigned char> readRawDiskBlocks(uint32_t startingDiskBlockNum, uint32_t N)
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

    /**
     * Retreive and return all blocks of the given key.
     * 
     * NOTE:
     *
     * `readBuffer` is the buffer we read the raw block data into.
     * 
     * We pass this in so that the data the block pointers reference
     * does not get de-allocated.
     */
    std::vector<Block> readBlocks(std::string key, std::vector<unsigned char> &readBuffer)
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

        uint32_t numBlocks = getNumBlocks(numBytes);
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
     * NOTE: 
     * 
     * If `key` already exists, we overwrite its
     * existing blocks and BAT entry.
     */
    void writeBlocks(std::string key, std::vector<Block> dataBlocks)
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
            uint32_t oldN = getNumBlocks(existingBatEntry->numBytes);
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

        for (auto &dataBlock : dataBlocks)
        {
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

    void deleteBlocks(std::string key)
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
        uint32_t N = getNumDiskBlocks(batEntry->numBytes, this->header.diskBlockSize);

        this->freeSpaceMap.freeNBlocks(startingDiskBlockNum, N);

        // remove bat entry
        this->bat.numEntries--;
        this->bat.table.erase(batEntry);
    }

    /**
     * Deletes the current store file and directory.
     */
    static void removeStoreFileAndDirectory(fs::path storeDirPath, std::string storeFileName)
    {
        fs::path storeFilePath = storeDirPath / storeFileName;

        if (!fs::exists(storeFilePath))
        {
            // std::cout << "Store file to be removed does not exist: " + storeFilePath.string() << std::endl;
            return;
        }

        // remove store file
        if (!fs::remove(storeFilePath))
            throw std::runtime_error(
                "file couldn't be removed: " + storeFilePath.string());
        
        // remove store dir
        if (!fs::remove(storeDirPath))
            throw std::runtime_error(
                "directory couldn't be removed: " + storeFilePath.string());
    }

private:
    fs::path storeDirPath;
    std::string storeFileName;

    fs::path storeFilePath;
    std::fstream storeFile;

    /**
     * Returns offset of disk block `diskBlockNum`
     */
    uint32_t getDiskBlockOffset(uint32_t diskBlockNum)
    {
        return this->header.blockStoreOffset + (this->header.diskBlockSize * diskBlockNum);
    }

    uint32_t getNumDiskBlocks(uint32_t numBytes, uint32_t blockSize)
    {
        return MathUtils::ceilDiv(numBytes, blockSize);
    }

    /**
     * Initialises the header.
     * 
     * NOTE: 
     * 
     * We don't write out to disk yet, as we must ensure the store file has been
     * created.
     */
    void initialiseHeader()
    {
        uint32_t batOffset = sizeof(Header);
        uint32_t numBlocks = getNumBlocks(this->maxDataSize);
        uint32_t batSize = sizeof(uint32_t) + (numBlocks * sizeof(BATEntry));
        uint32_t blockStoreOffset = sizeof(Header) + batSize;

        this->header = Header(
            this->magicNumber, 
            batOffset, 
            batSize,
            this->diskBlockSize, 
            this->maxDataSize, 
            blockStoreOffset
        );
    }

    /**
     * Returns true if the local copy of the header is valid, false otherwise.
     */
    bool headerValid()
    {
        return this->header.magicNumber == this->magicNumber;
    }

    /**
     * Returns true if the store directory and store file validly exist, false otherwise
     */
    bool storeFileExists()
    {
        // check store directory exists
        if (!(fs::exists(this->storeDirPath) && fs::is_directory(this->storeDirPath)))
        {
            std::cout << "Store directory does not exist: " << this->storeDirPath << std::endl;
            return false;
        }

        // check store file exists
        if (!fs::exists(this->storeFilePath))
        {
            std::cout << "Store file does not exist: " << this->storeFilePath << std::endl;
            return false;
        }
        return true;
    }

    /**
     * Creates a new store file in a new store directory.
     * 
     * NOTE: must be called AFTER initialiseHeader()
     */
    void createStoreFile()
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
};

////////////////////////////////////////////
// DiskStorage tests
////////////////////////////////////////////
namespace DiskStorageTests
{
    void setup()
    {
        // remove existing store of a previous test
        DiskStorage::removeStoreFileAndDirectory(fs::path("rackkey"), "store");
    }

    void teardown()
    {
        // remove store created during current test
        DiskStorage::removeStoreFileAndDirectory(fs::path("rackkey"), "store");
    }

    void testCanWriteAndReadNewHeaderAndBat()
    {
        setup();

        // implictly creates and writes header
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, 4192, 1u << 30);

        // create and write BAT
        int N = 3;
        for (int i = 0; i < N; i++)
        {
            BATEntry be(i, i*10, 10);
            ds.bat.table.push_back(be);
        }
        ds.bat.numEntries = N;
        ds.writeBAT();

        Header oldHeader = ds.header;
        BAT oldBat = ds.bat;

        // instantiate new object so don't have header or BAT cached (i.e. must read from disk)
        DiskStorage newDs = DiskStorage("rackkey", "store", 0xABABABAB, 4192, 1u << 30);

        Header newHeader = newDs.header;
        BAT newBat = ds.bat;

        assert(oldHeader.equals(newHeader));
        assert(oldBat.equals(newBat));

        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCCESS" << std::endl;

        teardown();
    }

    /**
     * Tests that we can write a single key's blocks to disk and read them back out.
     */
    void testCanWriteAndReadOneKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

        std::string key = "archive.zip";
        uint32_t N = 2;
        uint32_t numBytes = N * blockSize + 10;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        // write all blocks
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        // instantiate new object so don't have header, bat or file stream cached (i.e. must read from disk)
        DiskStorage newDs = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

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
            assert(writeBlocks[i].equals(readBlocks[i]));
        
        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCESS" << std::endl;
    
        teardown();
    }

    /**
     * Tests that we can write multiple keys' blocks to disk and read them back out.
     */
    void testCanWriteAndReadMultipleKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

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
        DiskStorage newDs = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

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
            assert(expectedBlocks.size() == readBlocks.size());
            for (uint32_t j = 0; j < expectedBlocks.size(); j++) 
            {
                assert(expectedBlocks[j].equals(readBlocks[j]));
            }

            assert(totalWriteBuffer == readBuffer);
        }

        std::cerr << "Test: " << __FUNCTION__ << " - SUCCESS" << std::endl;

        std::cout << newDs.header.toString() << std::endl;
        std::cout << newDs.bat.toString() << std::endl;

        teardown();
    }

    void testCanDeleteOneKeysBlocks()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

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
            assert(ds.freeSpaceMap.isMapped(i));
        assert(ds.bat.numEntries == 1);

        // delete blocks
        ds.deleteBlocks(key);

        // first (N + 1) blocks should now be free
        for (int i = 0; i < N + 1; i++)
            assert(!ds.freeSpaceMap.isMapped(i));

        assert(ds.bat.numEntries == 0 && ds.bat.table.size() == 0);

        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCCESS" << std::endl;

        teardown();
    }

    void testCanOverwriteExistingKey()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

        std::string key = "archive.zip";
        uint32_t N = 5;
        uint32_t numBytes = N * blockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        // write N blocks
        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key));

        // ensure blocks were correctly written
        assert((*entry)->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            assert(ds.freeSpaceMap.isMapped(i));
        
        // overwrite with M < N blocks
        uint32_t M = N - 2;
        numBytes = M * blockSize;
        writeDataBuffers.clear();
        writeBlocks = BlockUtils::generateNRandom(key, M, blockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        entry = ds.bat.findBATEntry(Crypto::sha256_32(key));

        // ensure didn't write a new entry (i.e. ensure we overwrote the old entry)
        assert(ds.bat.numEntries == 1);
        assert((*entry)->keyHash == Crypto::sha256_32(key));

        // ensure new blocks were correctly written
        assert((*entry)->numBytes == numBytes);
        for (int i = 0; i < M; i++)
            assert(ds.freeSpaceMap.isMapped(i));
        
        // ensure old blocks were de-allocated
        for (int i = M; i < N; i++)
            assert(!ds.freeSpaceMap.isMapped(i));
        
        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCCESS" << std::endl;
        
        teardown();
    }

    void testFragmentedWrite()
    {
        setup();

        uint32_t blockSize = 20;
        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, blockSize, 1u << 30);

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
            assert(!ds.freeSpaceMap.isMapped(i));

        // ensure `key2`s and `key3`s blocks are mapped
        for (int i = N; i < N + M + (N + 1); i++)
            assert(ds.freeSpaceMap.isMapped(i));

        // ensure `key3`s blocks start at block N + M
        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key3));
        assert((*entry)->startingDiskBlockNum == N + M);

        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCCESS" << std::endl;

        teardown();
    }

    void testMaxBlocksReached()
    {
        setup();

        DiskStorage ds = DiskStorage("rackkey", "store", 0xABABABAB, 4096, 1u << 20);

        /**
         * 1MB max data size (2^20), 4KB block size (2^12) -> 256 (2^8) blocks
         */
        uint32_t maxNumBlocks = ds.getNumBlocks(ds.maxDataSize);
        assert(maxNumBlocks == 256);

        // should successfully write N blocks
        std::string key = "archive.zip";
        uint32_t N = 230;
        uint32_t numBytes = N * ds.diskBlockSize;
        std::vector<std::vector<unsigned char>> writeDataBuffers;

        std::vector<Block> writeBlocks = BlockUtils::generateNRandom(key, N, ds.diskBlockSize, numBytes, writeDataBuffers);
        ds.writeBlocks(key, writeBlocks);

        auto entry = ds.bat.findBATEntry(Crypto::sha256_32(key));
        auto batEntry = *entry;

        // ensure first write was valid
        assert(batEntry->startingDiskBlockNum == 0);
        assert(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            assert(ds.freeSpaceMap.isMapped(i));

        // should fail to write 1 more block than is available
        std::string newKey = "video.mp4";
        uint32_t newN = (maxNumBlocks - N) + 1;
        uint32_t newNumBytes = newN * ds.diskBlockSize;
        writeDataBuffers.clear();

        writeBlocks = BlockUtils::generateNRandom(newKey, newN, ds.diskBlockSize, newNumBytes, writeDataBuffers);
        try 
        {
            ds.writeBlocks(newKey, writeBlocks);
            
            // shouldn't reach this point - investigate
            std::cout << ds.bat.toString() << std::endl;
            std::cout << ds.freeSpaceMap.toString() << std::endl;

            std::cerr << "Test: " << __FUNCTION__ << " - " << "FAILED" << std::endl;
            return;
        }
        catch (std::runtime_error& e)
        {
            std::cout << e.what() << std::endl;
        }

        // ensure disk state has been maintained
        assert(batEntry->startingDiskBlockNum == 0);
        assert(batEntry->numBytes == numBytes);
        for (int i = 0; i < N; i++)
            assert(ds.freeSpaceMap.isMapped(i));

        std::cerr << "Test: " << __FUNCTION__ << " - " << "SUCCESS" << std::endl;
    }

    /**
     * Testing that, on a failed write, we restore the
     * old, valid disk state.
     */
    void testRestoreDiskStateOnFailedWrite()
    {

    }

    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "DiskStorageTests" << std::endl;
        std::cerr << "###################################" << std::endl;

        testCanWriteAndReadNewHeaderAndBat();
        testCanWriteAndReadOneKeysBlocks();
        testCanWriteAndReadMultipleKeysBlocks();
        testCanOverwriteExistingKey();
        testFragmentedWrite();
        testMaxBlocksReached();
        testRestoreDiskStateOnFailedWrite();
    }
}

int main()
{
    DiskStorageTests::runAll();

    // FreeSpaceMapTests::testFreeNBlocks();
    // FreeSpaceMapTests::testAllocateNBlocks();
    // FreeSpaceMapTests::testAllocateThenFree();

}

/*
Immediate todo:
    - helper function for opening and closing the file 
    - checks in server.cpp that:
        - blocks are in correct order (disk_storage presumes they are)
        - first (blockNum - 1) blocks are full
    - have a go at using it to save blocks and retreive them
    - handle hash collisions:
        - heh?
    - handle concurrent r/w of storeFile (below)

Maybe:
    - handle concurrent r/w
        - i.e. use locks to prevent multiple threads
          r/w store file at same time
        - master may have multiple threads call same node
          concurrently, in which case multiple storage server
          threads will access same DiskStorage simultaneously
        - can probably just lock the entire DiskStorage for now
        - better approach:
            - when read, obtain a shared lock 
                - wait for any exclusive lock
            - when write, obtain an exclusive lock
                - wait for any exclusive AND shared lock
    
General cleanup:
    - make helper method for opening and closing store file
        - looks the same each time we do it
    - clean up DiskStorage
        - seems too large
        - file path stuff and private methods are ugly
        - perhaps can abstract the file path stuff to another class
    - nice explanation at top of disk_storage.cpp/.hpp
    - emphasise difference between diskBlocks and dataBlocks
        - ehhh...we are now only storing the data in our 
          on-disk blocks, so there really is no distinction
        - only potential complexity is that:
            - for us, data block size == disk block size (always)
            - think through consequences of allowing data vs disk block
              size to be difference
    - storage vs store naming? (DiskStorage -> StoreFile)?
*/