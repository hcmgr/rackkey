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
    /**
     * master-server-specific config
     */
    json::value masterServer = this->jsonConfig.at(U("masterServer"));

    this->masterServerIPPort = masterServer.at(U("masterServerIPPort")).as_string();

    auto nodesArray = masterServer.at(U("storageNodeIPs")).as_array();
    for (const auto& node : nodesArray) {
        this->storageNodeIPs.push_back(node.as_string());
    }
    this->numStorageNodes = this->storageNodeIPs.size();

    this->healthCheckPeriodMs = masterServer.at(U("healthCheckPeriodMs")).as_integer();

    this->numVirtualNodes = masterServer.at(U("numVirtualNodes")).as_integer();

    this->replicationFactor = masterServer.at(U("replicationFactor")).as_integer();

    /**
     * shared config
     */
    json::value shared = this->jsonConfig.at("shared");

    this->dataBlockSize = shared.at(U("dataBlockSize")).as_integer();
    this->keyLengthMax = shared.at(U("keyLengthMax")).as_integer();
}