// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

/**@file
 * Wrapper for the original Json11 unit tests source, making use of nx_kit test.h framework.
 */

#include <nx/kit/test.h>

#include <nx/kit/json.h>

//-------------------------------------------------------------------------------------------------
// Tests added by nx_kit: test that nx::kit::Json is in the proper namespace.

namespace test {

TEST(json, nx_kit_added)
{
    std::string err;
    const std::string jsonString = "[42, true]";
    const ::nx::kit::Json json = ::nx::kit::Json::parse(jsonString, err);
    const std::string serializedJson = json.dump();
    std::cerr << "Serialized Json: \"" << serializedJson << "\"" << std::endl;
    ASSERT_STREQ("", err);
    ASSERT_STREQ(jsonString, serializedJson);
}

} // namespace test

//-------------------------------------------------------------------------------------------------
// Original Json11 tests.

namespace json11 {} //< The original test source contains `using namespace json11;`.

#define JSON11_TEST_CUSTOM_CONFIG 1

#define JSON11_TEST_CPP_PREFIX_CODE namespace nx { namespace kit { namespace test {
#define JSON11_TEST_CPP_SUFFIX_CODE } /*<namespace test*/ } /*<namespace kit*/ } /*<namespace nx*/
#define JSON11_TEST_STANDALONE_MAIN 0
#define JSON11_TEST_CASE(NAME) TEST(json, NAME)
#define JSON11_TEST_ASSERT(CONDITION) ASSERT_TRUE(CONDITION)

#if defined(_WIN32)
    // Disable warnings "declaration of ... hides previous local declaration".
    #pragma warning(disable: 4456)
#endif

#include "../../src/json11/test.cpp" //< Original Json11 unit tests source.
