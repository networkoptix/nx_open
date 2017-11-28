#include <functional>
#include <gtest/gtest.h>
#include <nx/network/http/test_http_server.h>
#include <nx/update/info/detail/data_provider/async_raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>

#include "../../../inl.h"

namespace nx {
namespace update {
namespace info {
namespace detail {
namespace data_provider {
namespace test {

class TestProviderHandler: public AbstractAsyncRawDataProviderHandler
{
public:
    virtual void onUpdatesMetaInformationDone(
        ResultCode /*resultCode*/,
        const QByteArray& /*rawData*/) override
    {

    }

    virtual void onSpecificDone(
        ResultCode /*resultCode*/,
        const QByteArray& /*rawData*/) override
    {

    }

private:
};

class AsyncJsonDataProvider: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        prepareHttpServer();
        prepareDataProvider();
    }

private:
    AsyncRawDataProviderFactory m_factory;
    AbstractAsyncRawDataProviderPtr m_rawDataProvider;
    TestProviderHandler m_providerHandler;
    TestHttpServer m_httpServer;

    void prepareHttpServer()
    {
        using namespace std::placeholders;

        m_httpServer.registerRequestProcessorFunc(
            "/update.json",
            std::bind(&AsyncJsonDataProvider::onMetaRequest, this, _1, _2, _3, _4, _5));
        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void prepareDataProvider()
    {
        m_rawDataProvider = m_factory.create("127.0.0.1", &m_providerHandler);
        ASSERT_TRUE((bool) m_rawDataProvider);
    }

    void onMetaRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler /*completionHandler*/)
    {

    }
};

} // namespace test
} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
