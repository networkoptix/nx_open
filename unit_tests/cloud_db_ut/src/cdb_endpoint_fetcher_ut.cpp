/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <future>

#include <utils/common/cpp14.h>
#include <nx/network/cloud/cdb_endpoint_fetcher.h>

#include <gtest/gtest.h>


namespace nx {
namespace cdb {
namespace cl {

TEST(CloudModuleEndPointFetcher, common)
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
            if (resCode != nx_http::StatusCode::ok)
                int x = 0;
            cdbEndpoint = endpoint;
        });
    ASSERT_EQ(endpointFuture.get(), nx_http::StatusCode::ok);
}

}   //cl
}   //cdb
}   //nx
