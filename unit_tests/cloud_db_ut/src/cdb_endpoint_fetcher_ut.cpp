/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <utils/network/cloud_connectivity/cdb_endpoint_fetcher.h>

#include <future>

#include <gtest/gtest.h>


namespace nx {
namespace cdb {
namespace cl {

TEST(CloudModuleEndPointFetcher, common)
{
    nx::cc::CloudModuleEndPointFetcher endPointFetcher("cdb");
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
    endpointFuture.wait();
    ASSERT_EQ(endpointFuture.get(), nx_http::StatusCode::ok);
}

}   //cl
}   //cdb
}   //nx
