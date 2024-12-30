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

    void BlockNumList::serialize(std::vector<unsigned char> &buffer)
    {
        for (auto &bn : blockNums)
        {
            auto start = reinterpret_cast<unsigned char*>(&bn);
            auto end = start + sizeof(bn);
            buffer.insert(buffer.end(), start, end);        
        }
    }

    BlockNumList BlockNumList::deserialize(std::vector<unsigned char> &buffer)
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
        if (blockNums.size() != other.blockNums.size())
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

    SizeInfo::SizeInfo(
        uint32_t dataUsedSize,
        uint32_t dataTotalSize
    ) 
        : dataUsedSize(dataUsedSize),
          dataTotalSize(dataTotalSize)
    {
    }

    void SizeInfo::serialize(std::vector<unsigned char> &buffer)
    {
        // data used 
        auto start = reinterpret_cast<unsigned char*>(&dataUsedSize);
        auto end = start + sizeof(dataUsedSize);
        buffer.insert(buffer.end(), start, end);

        // data total
        start = reinterpret_cast<unsigned char*>(&dataTotalSize);
        end = start + sizeof(dataTotalSize);
        buffer.insert(buffer.end(), start, end);
    }

    SizeInfo SizeInfo::deserialize(std::vector<unsigned char> &buffer)
    {
        auto it = buffer.begin();

        uint32_t dataUsedSize;
        uint32_t dataTotalSize;

        std::memcpy(&dataUsedSize, &(*it), sizeof(dataUsedSize));
        it += sizeof(dataUsedSize);

        std::memcpy(&dataTotalSize, &(*it), sizeof(dataTotalSize));
        it += sizeof(dataTotalSize);

        assert(it == buffer.end());

        SizeInfo sr(dataUsedSize, dataTotalSize);
        return sr;
    }

    SizeInfo SizeInfo::deserialize(
        std::vector<unsigned char>::iterator &it,
        std::vector<unsigned char>::iterator end
    )
    {
        uint32_t dataUsedSize;
        uint32_t dataTotalSize;

        std::memcpy(&dataUsedSize, &(*it), sizeof(dataUsedSize));
        it += sizeof(dataUsedSize);

        std::memcpy(&dataTotalSize, &(*it), sizeof(dataTotalSize));
        it += sizeof(dataTotalSize);

        assert(it == end);

        SizeInfo sr(dataUsedSize, dataTotalSize);
        return sr;
    }

    bool SizeInfo::equals(SizeInfo other)
    {
        return (
            dataUsedSize == other.dataUsedSize && 
            dataTotalSize == other.dataTotalSize
        );
    }

    std::string SizeInfo::toString()
    {
        std::ostringstream oss;
        oss << "DataUsedSize: " << dataUsedSize << "\n";
        oss << "DataTotalSize: " << dataTotalSize << "\n";
        return oss.str();
    }

    ////////////////////////////////////////////
    // SyncResponse methods
    ////////////////////////////////////////////

    SyncInfo::SyncInfo(
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap,
        SizeInfo sizeInfo
    )
        : keyBlockNumMap(keyBlockNumMap),
          sizeInfo(sizeInfo)
    {
    }

    void SyncInfo::serialize(std::vector<unsigned char> &buffer)
    {
        // serialize { key -> block nums } map
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

        // serialize size info
        sizeInfo.serialize(buffer);
    }

    SyncInfo SyncInfo::deserialize(std::vector<unsigned char> &buffer)
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;

        auto it = buffer.begin();

        // deserialize { key -> block num } map
        while (it < (buffer.end() - sizeof(SizeInfo)))
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

        assert(it == (buffer.end() - sizeof(SizeInfo)));

        // deserialize size info
        SizeInfo sizeInfo = SizeInfo::SizeInfo::deserialize(it, buffer.end());
        assert(it == buffer.end());

        SyncInfo sr(std::move(keyBlockNumMap), std::move(sizeInfo));
        return sr;
    }

    bool SyncInfo::equals(SyncInfo other)
    {
        if (keyBlockNumMap.size() != other.keyBlockNumMap.size())
            return false;
        
        // { key -> block nums } map
        for (auto &p : keyBlockNumMap)
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

        // size info 
        if (!sizeInfo.equals(other.sizeInfo))
            return false;

        return true;
    }

    std::string SyncInfo::toString()
    {
        std::ostringstream oss;
        
        // { key -> block nums } map
        for (auto &p : keyBlockNumMap)
        {
            const std::string& key = p.first;
            const std::vector<uint32_t>& blockNums = p.second;
            
            oss << "Key: " << key << " -> Blocks: ";
            oss << PrintUtils::printVector(blockNums);
        }

        // size info
        oss << sizeInfo.toString() << "\n";
        
        return oss.str();
    }
};

namespace PayloadsTests
{
    void testBlockNumList()
    {
        std::vector<uint32_t> blockNums = {1, 2, 3, 4, 5};
        Payloads::BlockNumList original(blockNums);

        std::vector<unsigned char> buffer;
        original.serialize(buffer);

        Payloads::BlockNumList deserialized = Payloads::BlockNumList::deserialize(buffer);

        ASSERT_THAT(original.equals(deserialized));
    }

    void testSizeResponse()
    {
        Payloads::SizeInfo original(100, 500);

        std::vector<unsigned char> buffer;
        original.serialize(buffer);

        auto it = buffer.begin();
        Payloads::SizeInfo deserialized = Payloads::SizeInfo::deserialize(it, buffer.end());

        ASSERT_THAT(original.equals(deserialized));
    }

    void testSyncResponse()
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;
        keyBlockNumMap[StringUtils::fixedSize("file1", 50)] = {1, 2, 3};
        keyBlockNumMap[StringUtils::fixedSize("file2", 50)] = {4, 5, 6};
        Payloads::SizeInfo sizeInfo(10, 10);

        Payloads::SyncInfo original(keyBlockNumMap, sizeInfo);

        std::vector<unsigned char> buffer;
        original.serialize(buffer);

        Payloads::SyncInfo deserialized = Payloads::SyncInfo::deserialize(buffer);

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