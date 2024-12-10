#pragma once
#include <string>

/**
 * For the given `testFunc`, builds a pair of form:
 *      {function_name, function_pointer}
 */
#define TEST(testFunc) {#testFunc, testFunc}

/**
 * For the given `condition`, throw a runtime error
 * if it doesn't evaluate to true.
 */
#define ASSERT_THAT(condition) \
    if (!(condition)) throw std::runtime_error(std::string(#condition) + " failed at line: " + std::to_string(__LINE__))

/**
 * Testing library
 */
namespace TestUtils
{
    void runTest(std::string &testName, std::function<void()> &testFunc);
};