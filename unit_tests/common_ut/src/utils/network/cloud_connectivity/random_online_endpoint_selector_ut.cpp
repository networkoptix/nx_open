/**********************************************************
* Oct 8, 2015
* akolesnikov
***********************************************************/

#include <functional>
#include <future>
#include <vector>

#include <gtest/gtest.h>

#include <utils/network/cloud_connectivity/random_online_endpoint_selector.h>


TEST(RandomOnlineEndpointSelector, common)
{
    nx::cc::RandomOnlineEndpointSelector selector;
    std::promise<std::pair<nx_http::StatusCode::Value, SocketAddress>> promiseToSelect;

    std::vector<SocketAddress> endpoints;
#if 1
    endpoints.emplace_back("10.0.2.103", 80);
    //endpoints.emplace_back("93.158.134.3", 80);
    //endpoints.emplace_back("38.17.11.231", 80);
    //endpoints.emplace_back("38.17.11.232", 80);
#else
    endpoints.emplace_back("noptix.enk.me", 80);
    endpoints.emplace_back("ya.ru", 80);
    endpoints.emplace_back("qesdf32q4.com", 80);
    endpoints.emplace_back("nsdfn2340n.ru", 80);
#endif

    auto selected = promiseToSelect.get_future();

    selector.selectBestEndpont(
        "",
        endpoints,
        [&promiseToSelect](nx_http::StatusCode::Value result, SocketAddress endpoint){
            promiseToSelect.set_value(std::make_pair(result, std::move(endpoint)));
        });

    ASSERT_EQ(selected.get().first, nx_http::StatusCode::ok);
    ASSERT_TRUE(selected.get().second == endpoints[0] ||
                selected.get().second == endpoints[1]);
}
