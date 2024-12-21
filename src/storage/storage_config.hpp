#pragma once
#include "config.hpp"

class StorageConfig : public Config
{
public:
    StorageConfig(std::string configFilePath);
    void loadVariables();

    uint32_t diskBlockSize;
    uint32_t maxDataSizePower;
};