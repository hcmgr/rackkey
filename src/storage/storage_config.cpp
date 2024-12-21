#include <cpprest/json.h>
#include "storage_config.hpp"
using namespace web;

StorageConfig::StorageConfig(std::string configFilePath)
    : Config(configFilePath)
{
    this->loadVariables();
}

void StorageConfig::loadVariables()
{
    json::value storageServer = this->jsonConfig.at(U("storageServer"));

    this->diskBlockSize = storageServer.at(U("diskBlockSize")).as_integer();
    this->maxDataSizePower = storageServer.at(U("maxDataSizePower")).as_integer();
}

// void StorageConfig::loadVariables()
// {
//     this->diskBlockSize = this->jsonConfig.at(U("diskBlockSize")).as_integer();
//     this->maxDataSizePower = this->jsonConfig.at(U("maxDataSizePower")).as_integer();
// }