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

        std::vector<unsigned char> serialize();
        static BlockNumList deserialize(std::vector<unsigned char> buffer);
        bool equals(BlockNumList other);
    };

    /**
     * 
     */
    struct SizeResponse 
    {
        uint32_t dataUsedSize;
        uint32_t dataTotalSize;

        SizeResponse(
            uint32_t dataUsedSize, 
            uint32_t dataTotalSize
        );

        std::vector<unsigned char> serialize();
        static SizeResponse deserialize(std::vector<unsigned char> buffer);
        bool equals(SizeResponse other);
    };

    /**
     * 
     */
    struct SyncResponse
    {
        std::map<std::string, std::vector<uint32_t>> keyBlockNumMap;

        SyncResponse(
            std::map<std::string, std::vector<uint32_t>> keyBlockNumMap
        );

        std::vector<unsigned char> serialize();
        static SyncResponse deserialize(std::vector<unsigned char> buffer);
        bool equals(SyncResponse other);
    };
};

namespace PayloadsTests
{
    void runAll();

    void testSizeResponse();
    void testSyncResponse();
    void testBlockNumList();
}