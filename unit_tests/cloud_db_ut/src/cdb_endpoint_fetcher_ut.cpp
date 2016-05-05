/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>

#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <utils/common/cpp14.h>


namespace nx {
namespace cdb {
namespace cl {

TEST(CloudModuleEndPointFetcher, common)
{
    for (int i = 0; i < 7; ++i)
    {
        nx::network::cloud::CloudModuleEndPointFetcher endPointFetcher(
            "cdb",
            std::make_unique<nx::network::cloud::RandomEndpointSelector>());
        std::promise<nx_http::StatusCode::Value> endpointPromise;
        auto endpointFuture = endpointPromise.get_future();
        SocketAddress cdbEndpoint;
        endPointFetcher.get(
            [&endpointPromise, &cdbEndpoint](
                nx_http::StatusCode::Value resCode,
                SocketAddress endpoint)
            {
                endpointPromise.set_value(resCode);
                cdbEndpoint = endpoint;
            });
        const auto result = endpointFuture.get();
        if (result == nx_http::StatusCode::serviceUnavailable)
            continue;   //sometimes server reports 503
        ASSERT_EQ(result, nx_http::StatusCode::ok);
        return;
    }
    ASSERT_TRUE(false);
}

}   //cl
}   //cdb
}   //nx
