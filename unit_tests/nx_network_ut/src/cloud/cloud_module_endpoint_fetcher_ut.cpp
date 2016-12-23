/**********************************************************
* Sep 8, 2015
* akolesnikov
***********************************************************/

#include <future>

#include <gtest/gtest.h>

#include <nx/network/cloud/cdb_endpoint_fetcher.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>


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
        QUrl cdbEndpoint;
        endPointFetcher.get(
            [&endpointPromise, &cdbEndpoint](
                nx_http::StatusCode::Value resCode,
                QUrl endpoint)
            {
                cdbEndpoint = endpoint;
                endpointPromise.set_value(resCode);
            });
        const auto result = endpointFuture.get();
        endPointFetcher.pleaseStopSync();
        if (result == nx_http::StatusCode::serviceUnavailable)
            continue;   //sometimes server reports 503
        ASSERT_EQ(nx_http::StatusCode::ok, result);
        return;
    }
    ASSERT_TRUE(false);
}

TEST(CloudModuleEndPointFetcher, cancellation)
{
    for (int i = 0; i < 100; ++i)
    {
        nx::network::cloud::CloudModuleEndPointFetcher endPointFetcher(
            "cdb",
            std::make_unique<nx::network::cloud::RandomEndpointSelector>());

        for (int j = 0; j < 10; ++j)
        {
            nx::network::cloud::CloudModuleEndPointFetcher::ScopedOperation
                operation(&endPointFetcher);

            std::string s;
            operation.get(
                [&s](
                    nx_http::StatusCode::Value /*resCode*/,
                    QUrl endpoint)
                {
                    //if called after s desruction, will get segfault here
                    s = endpoint.toString().toStdString();
                });

            if (j == 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    nx::utils::random::number<int>(1, 299)));
        }

        endPointFetcher.pleaseStopSync();
    }
}

}   //cl
}   //cdb
}   //nx
