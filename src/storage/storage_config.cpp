#include "storage_config.hpp"

StorageConfig::StorageConfig(std::string configFilePath)
    : Config(configFilePath)
{
    this->loadVariables();
}

void StorageConfig::loadVariables()
{
    json::value storageConfig = this->jsonConfig.at(U("storageServer"));

    this->diskBlockSize = storageConfig.at(U("diskBlockSize")).as_integer();
    this->maxDataSizePower = storageConfig.at(U("maxDataSizePower")).as_integer();
}