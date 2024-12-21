#pragma once

#include <cpprest/json.h>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>

using namespace web;

/**
 * Represents our config as given by 'config.json'.
 */
class Config 
{
protected:
    /* Path to our config.json file */
    std::string configFilePath;

    /* JSON object we read our config.json file into */
    json::value jsonConfig;

    /**
     * Load config file into a `jsonConfig` (i.e. a useable JSON object).
     */
    void loadConfigFile();

public:
    /**
     * Load all variables from `jsonConfig` into member
     * variables of the current class.
     * 
     * NOTE:
     * 
     * As derived classes of Config, MasterConfig and StorageConfig 
     * implement their own loadVariables() functions that are called
     * in their respective constructors.
     */
    virtual void loadVariables() = 0;
    virtual ~Config() = default;

    /**
     * Param constructor
     */
    Config(std::string configFilePath);
};