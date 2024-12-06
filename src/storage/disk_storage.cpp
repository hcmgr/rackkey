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
    uint32_t blockSize;    
    uint32_t maxDataSize;
    uint32_t batOffset;    
    uint32_t batSize;       
    
    Header()
        : magicNumber(0),    
          batOffset(0),                
          batSize(0),                  
          blockSize(0),             
          maxDataSize(0)
    {
    }

    Header(
        uint32_t magicNumber, 
        uint32_t blockSize, 
        uint32_t maxDataSize,
        uint32_t batOffset, 
        uint32_t batSize
    )
        : magicNumber(magicNumber), 
          blockSize(blockSize), 
          maxDataSize(maxDataSize),
          batOffset(batOffset), 
          batSize(batSize)
    {
    }

    std::string toString() {
        std::stringstream ss;
        ss << "\nHeader:\n"
           << "  Magic Number: " << magicNumber << "\n"
           << "  Block Size: " << blockSize << "\n"
           << "  Max Data Size: " << maxDataSize << "\n"
           << "  BAT Offset: " << batOffset << "\n"
           << "  BAT Size: " << batSize;
        return ss.str();
    }
};

/**
 * Represents an entry in the BAT.
 */
struct __attribute__((packed)) BATEntry
{
    uint32_t keyHash;
    uint32_t firstBlockOffset;
    uint32_t numBytes;
    uint8_t isFree; // 1 if free, 0 otherwise

    BATEntry()
        : keyHash(0),
          firstBlockOffset(0),
          numBytes(0),
          isFree(1)
    {
    }

    BATEntry(uint32_t keyHash, 
        uint32_t firstBlockOffset, 
        uint32_t numBytes, 
        uint8_t isFree
    )
        : keyHash(keyHash),
          firstBlockOffset(firstBlockOffset),
          numBytes(numBytes),
          isFree(isFree)
    {
    }

    std::string toString() 
    {
        std::ostringstream oss;

        oss << "    keyHash: 0x" << std::hex << std::setw(8) << std::setfill('0') << keyHash << "\n"
            << "    firstBlockOffset: " << std::dec << firstBlockOffset << "\n"
            << "    numBytes: " << numBytes << "\n"
            << "    isFree: " << (isFree ? "true" : "false");

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
    {
    }

    BAT(uint32_t numEntries)
        : numEntries(numEntries),
          table(std::vector<BATEntry>(numEntries))
    {
    }

    std::string toString() 
    {
        std::ostringstream oss;

        oss << "\nBAT:\n"
            << "  Num. entries: " << numEntries << "\n"
            << "  Table:\n";
        
        for (BATEntry be : table) {
            oss << be.toString() << "\n\n";
        }
        
        return oss.str();
    }
};

class DiskStorage
{
private:
    fs::path storeDirPath;
    std::string storeFileName;
    uint32_t magicNumber;
    uint32_t blockSize;
    uint32_t maxDataSize;

    fs::path storeFilePath;
    std::fstream storeFile;

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
            std::fstream::in | std::fstream::out);
        std::cout << "Created store file: " << this->storeFilePath << std::endl;
    }

    /**
     * Deletes the current store file and directory.
     */
    void deleteStoreFileAndDirectory()
    {
        // remove store file
        if (!fs::remove(this->storeFilePath))
            throw std::runtime_error(
                "File couldn't be removed - file not found: " + this->storeFilePath.string());
        
        // remove store dir
        if (!fs::remove(this->storeDirPath))
            throw std::runtime_error(
                "Directory couldn't be removed - directory not found OR not empty: " + this->storeFilePath.string());
    }

    /**
     * Initialises the header and writes it out to disk.
     */
    void initialiseHeader()
    {
        uint32_t batOffset = sizeof(Header);
        uint32_t numBlocks = ceil(this->maxDataSize / this->blockSize);
        uint32_t batSize = sizeof(uint32_t) + (numBlocks * sizeof(BATEntry));

        this->header = Header(
            this->magicNumber, 
            this->blockSize, 
            this->maxDataSize, 
            batOffset, 
            batSize
        );

        writeHeader();
    }

    /**
     * Returns true if the local copy of the header is valid, false otherwise.
     */
    bool headerValid()
    {
        return this->header.magicNumber == this->magicNumber;
    }

public:
    /* File header */
    Header header;

    /* Block allocation table */
    BAT bat;

    /* Free space map for our block store */
    std::vector<uint8_t> freeSpaceMap;

    /**
     * Default constructor
     */
    DiskStorage(
        std::string storeDirPath = "rackkey",
        std::string storeFileName = "store",
        uint32_t magicNumber = 0xABABABAB,
        uint32_t blockSize = 4096,
        uint32_t maxDataSize = 1u << 30
    )
        : storeDirPath(fs::path(storeDirPath)),
          storeFileName(storeFileName),
          magicNumber(magicNumber),
          blockSize(blockSize),
          maxDataSize(maxDataSize)
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
            createStoreFile();
            initialiseHeader();
        }
    }

    /**
     * Reads header from file and updates local copy (this->header).
     */
    void readHeader()
    {
        this->storeFile.open(storeFilePath, std::fstream::in);

        if (this->storeFile.is_open()) {
            this->storeFile.seekg(0);
            this->storeFile.read(reinterpret_cast<char*>(&this->header), sizeof(this->header));
            if (!headerValid())
            {
                std::cout << "Error reading header" << std::endl;
            }
            this->storeFile.close();
        } else {
            std::cerr << "Failed to open file for reading!" << std::endl;
        }
    }

    /**
     * Writes the local copy of the header (this->header) out to disk.
     */
    void writeHeader()
    {
        this->storeFile.open(this->storeFilePath, std::fstream::out);

        if (this->storeFile.is_open()) {
            this->storeFile.seekp(0);
            this->storeFile.write(reinterpret_cast<char*>(&this->header), sizeof(this->header));
            this->storeFile.flush();
            this->storeFile.close();
        } else {
            std::cerr << "Failed to open file for writing!" << std::endl;
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
            std::cerr << "Failed to open file for reading!" << std::endl;
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
            this->storeFile.close();
        } else {
            std::cerr << "Failed to open file for writing!" << std::endl;
        }
    }

    /**
     * Read the store file into a buffer of raw bytes.
     * 
     * NOTE: used for debugging purposes mostly; such 
     *       buffers can be printed nicely using
     *       PrintUtils::printVector() (see utils.hpp)
     */
    std::vector<unsigned char> readRawStoreFile()
    {
        std::vector<unsigned char> buffer;

        // open the file for reading in binary mode
        this->storeFile.open(storeFilePath, std::fstream::in);

        if (this->storeFile.is_open()) {
            // get length of file
            this->storeFile.seekg(0, std::fstream::end);
            std::streampos fileSize = this->storeFile.tellg(); 
            this->storeFile.seekg(0, std::fstream::beg); 

            // read the file content into the buffer
            buffer.resize(fileSize); 
            this->storeFile.read(reinterpret_cast<char*>(buffer.data()), fileSize);
            this->storeFile.close();
        } else {
            std::cerr << "Failed to open file for reading!" << std::endl;
        }

        return buffer;
    }

    /**
     * Finds and returns pointer to `key`'s corresponding BAT entry
     */
    std::optional<BATEntry*> findBATEntry(std::string key)
    {
        for (auto &be : this->bat.table)
        {
            if (be.keyHash == Crypto::sha256_32(key))
                return &be;
        }
        return std::nullopt;
    }

    /**
     * TODO:
     * 
     * Retreive and return all blocks of the given key.
     */
    std::vector<Block> getBlocks(std::string key)
    {
        /*
        Plan:
            - read header and BAT
            - find `key`'s BAT entry
            - read `numBytes` block-by-block into a buffer
                - validate each block read against free space map
            - create, populate and return std::vector<Block>
                - pointers point to buffer
        */

       std::vector<Block> blocks;
       return blocks;
    }

    /**
     * TODO:
     * 
     * Write the given blocks for the given key.
     * 
     * NOTE: 
     * 
     * If `key` already exists, we:
     *      - overwrite its BAT entry AND;
     *      - free the existing blocks AND;
     *      - write the new blocks
     */
    void writeBlocks(std::string key, std::vector<Block> blocks)
    {
        /*
        Plan:
            - using free space map, find N-block contiguous space
            - write data out block by block
                - update free space map each time
                - grow file as needed (should maybe just pre-allocate)
            - create/replace BAT entry and write BAT out
                - only do once we have validly written out all blocks
        */

        BATEntry *existingEntryPtr = nullptr;
        auto entry = findBATEntry(key);
        if (entry)
            existingEntryPtr = *entry;

        // TODO: get new first block number from free space map (guaranteed N-block cont. space after)
        int firstBlockNum = 0;

        // write out data block-by-block
        for (auto block : blocks)
        {
            // TODO: write block
        }

        if (existingEntryPtr)
        {
            // de-allocate existing blocks
            // allocate new blocks
            // update fields
        } else {
            // allocate new blocks
            // insert new entry

        }
    }
};

////////////////////////////////////////////
// DiskStorage tests
////////////////////////////////////////////
namespace DiskStorageTests
{
    void testCanWriteNewHeaderAndBat()
    {
        // implictly writes header
        DiskStorage ds = DiskStorage();

        ds.readHeader();
        
        // construct and write BAT
        int N = 3;
        for (int i = 0; i < N; i++)
        {
            BATEntry be(i, i*10, 10, 0);
            ds.bat.table.push_back(be);
        }
        ds.bat.numEntries = N;
        ds.writeBAT();
        ds.readBAT();

        std::cout << ds.header.toString() << std::endl;
        std::cout << ds.bat.toString() << std::endl;

        std::vector<unsigned char> buffer = ds.readRawStoreFile();
        PrintUtils::printVector(buffer);
    }

    void testCanReadExistingHeaderAndBat()
    {
        // implictly reads existing header and BAT
        DiskStorage ds = DiskStorage();
        std::cout << ds.header.toString() << std::endl;
        std::cout << ds.bat.toString() << std::endl;

        std::vector<unsigned char> buffer = ds.readRawStoreFile();
        PrintUtils::printVector(buffer);
    }
}

/*
Immediate todo:
    - rename DiskStorage to StoreFile
    - diskStorage.writeBlocks()
    - diskStorage.getBlocks()
    - have a go at using it to save blocks and retreive them
    - handle hash collisions:
        - heh?
    - probably pre-allocate the whole file at the start
        - write byte to last position (some EOF byte or something)
        - i.e. at maxDataSize + 1 position
*/

int main()
{
    // DiskStorageTests::testCanWriteNewHeaderAndBat();
    // DiskStorageTests::testCanReadExistingHeaderAndBat();

    // FreeSpaceMapTests::testFreeNBlocks();
    // FreeSpaceMapTests::testAllocateNBlocks();
    // FreeSpaceMapTests::testAllocateThenFree();
}