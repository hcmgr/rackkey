#pragma once

#include <cstdlib>
#include <vector>
#include <string>
#include <map>

/**
 * Container for all payload types sent between 
 * master server and storage server.
 */
namespace Payloads
{
    /**
     * 
     */
    struct BlockNumList
    {
        std::vector<uint32_t> blockNums;

        BlockNumList(
            std::vector<uint32_t> blockNums
        );

        void serialize(std::vector<unsigned char> &buffer);
        static BlockNumList deserialize(std::vector<unsigned char> &buffer);
        bool equals(BlockNumList other);
    };

    /**
     * 
     */
    struct __attribute__((packed)) SizeInfo 
    {
        uint32_t dataUsedSize;
        uint32_t dataTotalSize;

        SizeInfo(
            uint32_t dataUsedSize, 
            uint32_t dataTotalSize
        );

        void serialize(std::vector<unsigned char> &buffer);
        static SizeInfo deserialize(std::vector<unsigned char> &buffer);
        static SizeInfo deserialize(
            std::vector<unsigned char>::iterator &it,
            std::vector<unsigned char>::iterator end
        );
        bool equals(SizeInfo other);
        std::string toString();
    };

    /**
     * 
     */
    struct SyncInfo
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;
        SizeInfo sizeInfo;

        SyncInfo(
            std::map<std::string, std::vector<uint32_t>> keyBlockNumMap,
            SizeInfo sizeInfo
        );

        void serialize(std::vector<unsigned char> &buffer);
        static SyncInfo deserialize(std::vector<unsigned char> &buffer);
        bool equals(SyncInfo other);
        std::string toString();
    };
};

namespace PayloadsTests
{
    void runAll();

    void testSizeResponse();
    void testSyncResponse();
    void testBlockNumList();
}