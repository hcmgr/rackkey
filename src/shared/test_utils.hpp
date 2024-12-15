#pragma once
#include <string>
#include <functional>

/**
 * For the given `testFunc`, builds a pair of form:
 *      {function_name, function_pointer}
 */
#define TEST(testFunc) {#testFunc, testFunc}

/**
 * For the given `condition`, throw a runtime error
 * if it doesn't evaluate to true.
 * 
 * NOTE:
 *
 * Throwing a runtime error exits from the testing function immediately.
 * In future, might want to continue executing and report all errors.
 * For now, this is fine.
 */
#define ASSERT_THAT(condition) \
    if (!(condition)) \
        throw std::runtime_error(std::string(#condition) + " failed at line: " + std::to_string(__LINE__))

/**
 * Foribly fail the test by throwing a runtime error with the given `msg`.
 */
#define FORCE_FAIL(msg) \
    throw std::runtime_error(std::string(msg) + ", line: " + std::to_string(__LINE__))

/**
 * Testing library
 */
namespace TestUtils
{
    void runTest(std::string &testName, std::function<void()> &testFunc);
};