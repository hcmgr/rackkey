#pragma once
#include <string>
#include "config.hpp"

class StorageConfig: public Config 
{
public:
    StorageConfig(std::string configFilePath);
    void loadVariables() override;

    /* store directory ('rackkey/' by default) */
    std::string storeDirPath;

    /* store file prefix ('store' by default) */
    std::string storeFilePrefix;

    /* On-disk block size */
    uint32_t diskBlockSize;

    /**
     * log2 of store file's maximum data section size
     * 
     * i.e. 1u << maxDataSizePower == maximum data section size
     */
    uint32_t maxDataSizePower;

    /* True if should remove existing store file, false otherwise */
    bool removeExistingStoreFile;

    /**
     * Size of data (in bytes) each data block (i.e. Block object) stores.
     */
    uint32_t dataBlockSize;
};