/**********************************************************
* Oct 8, 2015
* akolesnikov
***********************************************************/

#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include <nx/network/cloud/random_online_endpoint_selector.h>
#include <nx/utils/future.h>


TEST(RandomOnlineEndpointSelector, common)
{
    if( SocketFactory::isStreamSocketTypeEnforced() )
        return;

    for (int i = 0; i < 20; ++i)
    {
        nx::utils::promise<std::pair<nx_http::StatusCode::Value, SocketAddress>> promiseToSelect;
        nx::network::cloud::RandomOnlineEndpointSelector selector;

        std::vector<SocketAddress> endpoints;
        endpoints.emplace_back("noptix.enk.me", 80);
        endpoints.emplace_back("ya.ru", 80);
        endpoints.emplace_back("qesdf32q4.com", 80);
        endpoints.emplace_back("nsdfn2340n.ru", 80);

        auto selected = promiseToSelect.get_future();

        selector.selectBestEndpont(
            "",
            endpoints,
            [&promiseToSelect](nx_http::StatusCode::Value result, SocketAddress endpoint){
                promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
            });

        const auto result = selected.get();

        ASSERT_EQ(result.first, nx_http::StatusCode::ok);
        ASSERT_TRUE(
            result.second == endpoints[0] ||
            result.second == endpoints[1]);
    }
}

TEST(RandomOnlineEndpointSelector, earlyCancellation)
{
    for (int i = 0; i < 20; ++i)
    {
        nx::utils::promise<std::pair<nx_http::StatusCode::Value, SocketAddress>> promiseToSelect;

        nx::network::cloud::RandomOnlineEndpointSelector selector;

        std::vector<SocketAddress> endpoints;
        endpoints.emplace_back("noptix.enk.me", 80);
        endpoints.emplace_back("ya.ru", 80);
        endpoints.emplace_back("qesdf32q4.com", 80);
        endpoints.emplace_back("nsdfn2340n.ru", 80);

        auto selected = promiseToSelect.get_future();

        selector.selectBestEndpont(
            "",
            endpoints,
            [&promiseToSelect](nx_http::StatusCode::Value result, SocketAddress endpoint) {
                promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
            });
    }
}

TEST(RandomOnlineEndpointSelector, selectError)
{
    for (int endpointNumber = 1; endpointNumber < 4; ++endpointNumber)
    {
        for (int i = 0; i < 20; ++i)
        {
            nx::utils::promise<std::pair<nx_http::StatusCode::Value, SocketAddress>> promiseToSelect;

            nx::network::cloud::RandomOnlineEndpointSelector selector;

            std::vector<SocketAddress> endpoints(
                endpointNumber,
                SocketAddress("qesdf32q4.com", 80));

            auto selected = promiseToSelect.get_future();

            selector.selectBestEndpont(
                "",
                endpoints,
                [&promiseToSelect](nx_http::StatusCode::Value result, SocketAddress endpoint) {
                    promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
                });

            const auto result = selected.get();

            ASSERT_NE(result.first, nx_http::StatusCode::ok);
        }
    }
}
