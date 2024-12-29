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

    this->storeDirPath = storageConfig.at(U("storeDirPath")).as_string();
    this->storeFilePrefix = storageConfig.at(U("storeFilePrefix")).as_string();
    this->diskBlockSize = storageConfig.at(U("diskBlockSize")).as_integer();
    this->maxDataSizePower = storageConfig.at(U("maxDataSizePower")).as_integer();
    this->removeExistingStoreFile = storageConfig.at(U("removeExistingStoreFile")).as_bool();

    /**
     * shared config
     */
    json::value shared = this->jsonConfig.at("shared");

    this->dataBlockSize = shared.at(U("dataBlockSize")).as_integer();
    this->keyLengthMax = shared.at(U("keyLengthMax")).as_integer();
}