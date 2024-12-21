#include <cpprest/json.h>
#include "master_config.hpp"

using namespace web;

MasterConfig::MasterConfig(std::string configFilePath)
    : Config(configFilePath)
{
    loadVariables();
}

void MasterConfig::loadVariables()
{
    json::value masterServer = this->jsonConfig.at(U("masterServer"));

    this->masterServerIPPort = masterServer.at(U("masterServerIPPort")).as_string();

    auto nodesArray = masterServer.at(U("storageNodeIPs")).as_array();
    for (const auto& node : nodesArray) {
        this->storageNodeIPs.push_back(node.as_string());
    }
    this->numStorageNodes = this->storageNodeIPs.size();

    this->healthCheckPeriodMs = masterServer.at(U("healthCheckPeriodMs")).as_integer();

    this->numVirtualNodes = masterServer.at(U("numVirtualNodes")).as_integer();

    this->dataBlockSize = masterServer.at(U("dataBlockSize")).as_integer();

    this->replicationFactor = masterServer.at(U("replicationFactor")).as_integer();
}

// void MasterConfig::loadVariables()
// {
//     // json::value masterServer = this->jsonConfig.at(U("masterServer"));

//     this->masterServerIPPort = this->jsonConfig.at(U("masterServerIPPort")).as_string();

//     auto nodesArray = this->jsonConfig.at(U("storageNodeIPs")).as_array();
//     for (const auto& node : nodesArray) {
//         this->storageNodeIPs.push_back(node.as_string());
//     }
//     this->numStorageNodes = this->storageNodeIPs.size();

//     this->healthCheckPeriodMs = this->jsonConfig.at(U("healthCheckPeriodMs")).as_integer();

//     this->numVirtualNodes = this->jsonConfig.at(U("numVirtualNodes")).as_integer();

//     this->dataBlockSize = this->jsonConfig.at(U("dataBlockSize")).as_integer();

//     this->replicationFactor = this->jsonConfig.at(U("replicationFactor")).as_integer();
// }