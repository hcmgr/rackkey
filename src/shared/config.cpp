#include <cpprest/json.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

#include "config.hpp"

using namespace web;

////////////////////////////////////////////
// Config methods
////////////////////////////////////////////

/**
 * Load config file into a `jsonConfig` (i.e. a useable JSON object).
 */
void Config::loadConfigFile() 
{
    std::ifstream fileStream(this->configFilePath);
    if (!fileStream.is_open()) {
        throw std::runtime_error("Unable to open configuration file: " + this->configFilePath);
    }

    std::string configContent((std::istreambuf_iterator<char>(fileStream)), std::istreambuf_iterator<char>());
    this->jsonConfig = web::json::value::parse(configContent);
}

/* Default constructor */
Config::Config(std::string configFilePath)
    : configFilePath(configFilePath)
{
    loadConfigFile();
}

