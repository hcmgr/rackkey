#include "storage_config.hpp"

StorageConfig::StorageConfig(std::string configFilePath)
    : Config(configFilePath)
{
    this->loadVariables();
}

void StorageConfig::loadVariables()
{
    /**
     * storage-server-specific config
     */
    json::value storageConfig = this->jsonConfig.at(U("storageServer"));

    this->diskBlockSize = storageConfig.at(U("diskBlockSize")).as_integer();
    this->maxDataSizePower = storageConfig.at(U("maxDataSizePower")).as_integer();

    /**
     * shared config
     */
    json::value shared = this->jsonConfig.at("shared");
    this->dataBlockSize = shared.at(U("dataBlockSize")).as_integer();
}