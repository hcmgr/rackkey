#include <string>
#include <iostream>
#include <iomanip>

#include "test_utils.hpp"

/**
 * Testing library
 */
namespace TestUtils
{
    const std::string GREEN = "\033[32m";
    const std::string RED = "\033[31m";
    const std::string RESET = "\033[0m";

    const int STATUS_COLUMN = 50; // column where 'SUCCESS' or 'FAILED' messages are printed

    void printSuccessMsg(const std::string &testName)
    {
        std::cerr << std::left << std::setw(STATUS_COLUMN) << testName
                  << GREEN << "SUCCESS" 
                  << RESET << std::endl;
    }

    void printFailMsg(const std::string &testName, const std::string &msg)
    {
        std::cerr << std::left << std::setw(STATUS_COLUMN) << testName
                  << RED << "FAILED" 
                  << RESET << ": " << msg << std::endl;
    }

    void runTest(std::string &testName, std::function<void()> &testFunc)
    {
        try
        {
            testFunc();
            printSuccessMsg(testName);
        }
        catch (const std::exception& e)
        {
            printFailMsg(testName, e.what());
        }
    }
};