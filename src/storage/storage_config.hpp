#pragma once
#include <string>
#include "config.hpp"

class StorageConfig: public Config 
{
public:
    StorageConfig(std::string configFilePath);
    void loadVariables() override;

    std::string storeDirPath;
    std::string storeFilePrefix;
    uint32_t diskBlockSize;
    uint32_t maxDataSizePower;
    bool removeExistingStoreFile;

    uint32_t dataBlockSize;
};