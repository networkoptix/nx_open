/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <cloud_db_client/src/cdb_endpoint_fetcher.h>

#include <future>

#include <gtest/gtest.h>


namespace nx {
namespace cdb {
namespace cl {

TEST(CloudModuleEndPointFetcher, common)
{
    CloudModuleEndPointFetcher endPointFetcher("cdb");
    std::promise<api::ResultCode> endpointPromise;
    auto endpointFuture = endpointPromise.get_future();
    SocketAddress cdbEndpoint;
    endPointFetcher.get(
        [&endpointPromise, &cdbEndpoint](api::ResultCode resCode, SocketAddress endpoint){
            endpointPromise.set_value(resCode);
            cdbEndpoint = endpoint;
        });
    endpointFuture.wait();
    ASSERT_EQ(endpointFuture.get(), api::ResultCode::ok);
}

}   //cl
}   //cdb
}   //nx
