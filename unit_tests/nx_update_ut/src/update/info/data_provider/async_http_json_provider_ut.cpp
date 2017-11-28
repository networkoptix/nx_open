#include <functional>
#include <gtest/gtest.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/update/info/detail/data_provider/async_raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/abstract_async_raw_data_provider_handler.h>
#include <rest/server/json_rest_handler.h>

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
        ResultCode resultCode,
        const QByteArray& rawData) override
    {
        ASSERT_EQ(ResultCode::ok, resultCode);
        QnMutexLocker lock(&m_mutex);
        m_metaData = rawData;
        m_condition.wakeOne();;
    }

    virtual void onSpecificDone(
        ResultCode resultCode,
        const QByteArray& rawData) override
    {
        ASSERT_EQ(ResultCode::ok, resultCode);
        QnMutexLocker lock(&m_mutex);
        m_specificData = rawData;
        m_condition.wakeOne();;
    }

    QByteArray metaData()
    {
        QnMutexLocker lock(&m_mutex);
        while (m_metaData.isEmpty())
            m_condition.wait(lock.mutex());

        return m_metaData;
    }

    QByteArray updateData()
    {
        QnMutexLocker lock(&m_mutex);
        while (m_specificData.isEmpty())
            m_condition.wait(lock.mutex());

        return m_specificData;
    }

private:
    QnMutex m_mutex;
    QnWaitCondition m_condition;

    QByteArray m_metaData;
    QByteArray m_specificData;
};

class AsyncJsonDataProvider: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        prepareHttpServer();
        prepareDataProvider();
    }

    void whenAsyncRequestIssued()
    {
        m_rawDataProvider->getUpdatesMetaInformation();
    }

    void thenCorrectDataIsProvided()
    {
        ASSERT_EQ(metaDataJson, m_providerHandler.metaData());
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
        nx_http::RequestProcessedHandler completionHandler)
    {
        QnJsonRestResult response;
        response.error = QnRestResult::Error::NoError;

        nx_http::RequestResult requestResult(nx_http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<nx_http::BufferSource>(
            "application/json",
            metaDataJson);

        completionHandler(std::move(requestResult));
    }
};

TEST_F(AsyncJsonDataProvider, MetaDataProvided)
{
    whenAsyncRequestIssued();
    thenCorrectDataIsProvided();
}

} // namespace test
} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
