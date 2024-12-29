#include <iostream>

#include "payloads.hpp"
#include "test_utils.hpp"
#include "utils.hpp"

/**
 * Container for all payload types sent between 
 * master server and storage server.
 */
namespace Payloads
{
    ////////////////////////////////////////////
    // BlockNumList methods
    ////////////////////////////////////////////

    BlockNumList::BlockNumList(std::vector<uint32_t> blockNums)
        : blockNums(blockNums)
    {
    }

    std::vector<unsigned char> BlockNumList::serialize()
    {
        std::vector<unsigned char> buffer;
        for (auto &bn : this->blockNums)
        {
            auto start = reinterpret_cast<unsigned char*>(&bn);
            auto end = start + sizeof(bn);
            buffer.insert(buffer.end(), start, end);        }

        return buffer;
    }

    BlockNumList BlockNumList::deserialize(std::vector<unsigned char> buffer)
    {
        std::vector<uint32_t> blockNums;
        auto it = buffer.begin();
        while (it < buffer.end())
        {
            uint32_t blockNum;
            std::memcpy(&blockNum, &(*it), sizeof(blockNum));
            blockNums.push_back(blockNum);

            it += sizeof(blockNum);
        }

        assert(it == buffer.end());

        BlockNumList bnl(blockNums);
        return bnl;
    }

    bool BlockNumList::equals(BlockNumList other)
    {
        if (this->blockNums.size() != other.blockNums.size())
            return false;
        
        for (int i = 0; i < blockNums.size(); i++)
        {
            if (blockNums[i] != other.blockNums[i])
                return false;
        }
        return true;
    }

    ////////////////////////////////////////////
    // SizeResponse methods
    ////////////////////////////////////////////

    SizeResponse::SizeResponse(
        uint32_t dataUsedSize,
        uint32_t dataTotalSize
    ) 
        : dataUsedSize(dataUsedSize),
          dataTotalSize(dataTotalSize)
    {
    }

    std::vector<unsigned char> SizeResponse::serialize()
    {
        std::vector<unsigned char> buffer;
        
        // data used 
        auto start = reinterpret_cast<unsigned char*>(&dataUsedSize);
        auto end = start + sizeof(dataUsedSize);
        buffer.insert(buffer.end(), start, end);

        // data total
        start = reinterpret_cast<unsigned char*>(&dataTotalSize);
        end = start + sizeof(dataTotalSize);
        buffer.insert(buffer.end(), start, end);

        return buffer;
    }

    SizeResponse SizeResponse::deserialize(std::vector<unsigned char> buffer)
    {
        uint32_t dataUsedSize;
        uint32_t dataTotalSize;

        auto it = buffer.begin();

        std::memcpy(&dataUsedSize, &(*it), sizeof(dataUsedSize));
        it += sizeof(dataUsedSize);

        std::memcpy(&dataTotalSize, &(*it), sizeof(dataTotalSize));
        it += sizeof(dataTotalSize);

        assert(it == buffer.end());

        SizeResponse sr(dataUsedSize, dataTotalSize);
        return sr;
    }

    bool SizeResponse::equals(SizeResponse other)
    {
        return (
            dataUsedSize == other.dataUsedSize && 
            dataTotalSize == other.dataTotalSize
        );
    }

    ////////////////////////////////////////////
    // SyncResponse methods
    ////////////////////////////////////////////

    SyncResponse::SyncResponse(
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap
    )
        : keyBlockNumMap(keyBlockNumMap)
    {
    }

    std::vector<unsigned char> SyncResponse::serialize()
    {
        std::vector<unsigned char> buffer;

        for (auto &p : keyBlockNumMap)
        {
            std::string key = p.first;
            std::vector<uint32_t> blockNums = p.second;

            uint32_t numBlocks = blockNums.size();

            // key
            buffer.insert(buffer.end(), key.begin(), key.end());

            // num. blocks
            auto start = reinterpret_cast<unsigned char*>(&numBlocks);
            auto end = start + sizeof(numBlocks);
            buffer.insert(buffer.end(), start, end);

            // block nums
            for (auto &bn : blockNums)
            {
                start = reinterpret_cast<unsigned char*>(&bn);
                end = start + sizeof(bn);
                buffer.insert(buffer.end(), start, end);
            }
        }

        return buffer;
    }

    SyncResponse SyncResponse::deserialize(std::vector<unsigned char> buffer)
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;

        auto it = buffer.begin();
        while (it < buffer.end())
        {
            // key
            std::string key(it, it + 50);
            it += key.size();

            // num. blocks
            uint32_t numBlocks;
            std::memcpy(&numBlocks, &(*it), sizeof(numBlocks));
            it += sizeof(numBlocks);

            // block nums
            std::vector<uint32_t> blockNums;
            for (int i = 0; i < numBlocks; i++)
            {
                uint32_t blockNum;
                std::memcpy(&blockNum, &(*it), sizeof(blockNum));
                blockNums.push_back(blockNum);
                it += sizeof(blockNum);
            }

            // add entry
            keyBlockNumMap[key] = std::move(blockNums);
        }
        
        assert(it == buffer.end());

        SyncResponse sr(std::move(keyBlockNumMap));
        return sr;
    }

    bool SyncResponse::equals(SyncResponse other)
    {
        if (this->keyBlockNumMap.size() != other.keyBlockNumMap.size())
            return false;
        
        for (auto &p : this->keyBlockNumMap)
        {
            std::string key = p.first;
            std::vector<uint32_t> blockNums = p.second;
            std::vector<uint32_t> otherBlockNums = other.keyBlockNumMap[key];

            if (blockNums.size() != otherBlockNums.size())
                return false;
            
            for (int i = 0; i < blockNums.size(); i++)
            {
                if (blockNums[i] != otherBlockNums[i])
                    return false;
            }
        }
        return true;
    }
};

namespace PayloadsTests
{
    void testBlockNumList()
    {
        std::vector<uint32_t> blockNums = {1, 2, 3, 4, 5};
        Payloads::BlockNumList original(blockNums);
        std::vector<unsigned char> serializedData = original.serialize();
        Payloads::BlockNumList deserialized = Payloads::BlockNumList::deserialize(serializedData);

        ASSERT_THAT(original.equals(deserialized));
    }

    void testSizeResponse()
    {
        Payloads::SizeResponse original(100, 500);
        std::vector<unsigned char> serializedData = original.serialize();
        Payloads::SizeResponse deserialized = Payloads::SizeResponse::deserialize(serializedData);

        ASSERT_THAT(original.equals(deserialized));
    }

    void testSyncResponse()
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;
        keyBlockNumMap[StringUtils::fixedSize("file1", 50)] = {1, 2, 3};
        keyBlockNumMap[StringUtils::fixedSize("file2", 50)] = {4, 5, 6};

        Payloads::SyncResponse original(keyBlockNumMap);
        std::vector<unsigned char> serializedData = original.serialize();
        Payloads::SyncResponse deserialized = Payloads::SyncResponse::deserialize(serializedData);

        ASSERT_THAT(original.equals(deserialized));
    }

    void runAll()
    {
        std::cerr << "###################################" << std::endl;
        std::cerr << "Payloads Tests" << std::endl;
        std::cerr << "###################################" << std::endl;

        std::vector<std::pair<std::string, std::function<void()>>> tests = {
            TEST(testBlockNumList),
            TEST(testSizeResponse),
            TEST(testSyncResponse)
        };

        for (auto &[name, func] : tests)
        {
            TestUtils::runTest(name, func);
        }
    }
}