#include <gtest/gtest.h>

#include <nx/network/http/rest/http_rest_client.h>

namespace nx {
namespace network {
namespace http {
namespace rest {
namespace test {

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
    nx::network::http::StringType resultPath;

    ASSERT_FALSE(substituteParameters(
        "/account/dummy/systems", &resultPath, {"ak"}));
    ASSERT_FALSE(substituteParameters(
        "/account/{accId}/systems/sys1", &resultPath, {"ak", "sys1"}));
}

} // namespace test
} // namespace rest
} // namespace nx
} // namespace network
} // namespace http
