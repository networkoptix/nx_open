// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/rest/http_rest_client.h>

namespace nx::network::http::rest::test {

TEST(HttpRest_substituteParameters, correct)
{
    ASSERT_EQ(
        "/account/akolesnikov/systems",
        substituteParameters("/account/{accountId}/systems", {"akolesnikov"}));

    ASSERT_EQ(
        "/account/akolesnikov/systems/sys1",
        substituteParameters("/account/{accountId}/systems/{systemId}", {"akolesnikov", "sys1"}));
}

TEST(HttpRest_substituteParameters, bad_input)
{
    std::string resultPath;

    ASSERT_FALSE(substituteParameters(
        "/account/dummy/systems", &resultPath, {"ak"}));
    ASSERT_FALSE(substituteParameters(
        "/account/{accId}/systems/sys1", &resultPath, {"ak", "sys1"}));
}

TEST(HttpRest_client, replaceAllParameters)
{
    ASSERT_EQ(
        "/users/.*/docs/.*",
        replaceAllParameters("/users/{userId}/docs/{docId}", ".*"));

    ASSERT_EQ("/foo/bar", replaceAllParameters("/foo/bar", ".*"));
}

} // namespace nx::network::http::rest::test
