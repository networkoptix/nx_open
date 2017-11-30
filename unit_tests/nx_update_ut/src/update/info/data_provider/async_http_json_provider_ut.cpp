#include <functional>
#include <gtest/gtest.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_builder.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
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
    virtual void onGetUpdatesMetaInformationDone(ResultCode resultCode, QByteArray rawData) override
    {
        QnMutexLocker lock(&m_mutex);
        m_lastResultCode = resultCode;
        m_metaData = std::make_unique<QByteArray>(std::move(rawData));
        m_condition.wakeOne();;
    }

    virtual void onGetSpecificUpdateInformationDone(
        ResultCode resultCode,
        QByteArray rawData) override
    {
        QnMutexLocker lock(&m_mutex);
        m_lastResultCode = resultCode;
        m_specificData = std::make_unique<QByteArray>(std::move(rawData));
        m_condition.wakeOne();;
    }

    QByteArray metaData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_metaData)
            m_condition.wait(lock.mutex());

        return *m_metaData;
    }

    QByteArray updateData()
    {
        QnMutexLocker lock(&m_mutex);
        while (!m_specificData)
            m_condition.wait(lock.mutex());

        return *m_specificData;
    }

    ResultCode lastResultCode() const
    {
        QnMutexLocker lock(&m_mutex);
        return m_lastResultCode;
    }
private:
    mutable QnMutex m_mutex;
    QnWaitCondition m_condition;

    ResultCode m_lastResultCode = ResultCode::ok;
    std::unique_ptr<QByteArray> m_metaData;
    std::unique_ptr<QByteArray> m_specificData;
};

static const QString kCustomization = "default";
static const QString kVersion = "16975";
static const QString kUpdatesPath = "/updates.json";
static const QString kUpdatePath = "/update.json";
static const QByteArray kJsonMimeType = "application/json";

class AsyncJsonDataProvider: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        prepareHttpServer();
        prepareDataProvider();
    }

    void whenGetUpdatesMetaInformationRequestIssued()
    {
        m_rawDataProvider->getUpdatesMetaInformation();
    }

    void whenGetSpecificUpdateInformationRequestIssued()
    {
        m_rawDataProvider->getSpecificUpdateData(kCustomization, kVersion);
    }

    void thenCorrectMetaDataIsProvided()
    {
        ASSERT_EQ(QByteArray(metaDataJson), m_providerHandler.metaData());
        ASSERT_EQ(ResultCode::ok, m_providerHandler.lastResultCode());
    }

    void thenCorrectUpdateDataIsProvided()
    {
        ASSERT_EQ(QByteArray(updateJson), m_providerHandler.updateData());
        ASSERT_EQ(ResultCode::ok, m_providerHandler.lastResultCode());
    }

private:
    AbstractAsyncRawDataProviderPtr m_rawDataProvider;
    TestProviderHandler m_providerHandler;
    TestHttpServer m_httpServer;

    void prepareHttpServer()
    {
        using namespace std::placeholders;

        m_httpServer.registerStaticProcessor(kUpdatesPath, metaDataJson, kJsonMimeType);
        QString updatePath = lit("/%1/%2%3").arg(kCustomization).arg(kVersion).arg(kUpdatePath);
        m_httpServer.registerStaticProcessor(updatePath, updateJson, kJsonMimeType);

        ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void prepareDataProvider()
    {
        nx::utils::Url metaUrl;
        metaUrl.setScheme("http");
        metaUrl.setHost(m_httpServer.serverAddress().address.toString());
        metaUrl.setPort(m_httpServer.serverAddress().port);

        m_rawDataProvider = RawDataProviderFactory::create(metaUrl.toString(), &m_providerHandler);
        ASSERT_TRUE((bool) m_rawDataProvider);
    }
};

TEST_F(AsyncJsonDataProvider, MetaDataProvided)
{
    whenGetUpdatesMetaInformationRequestIssued();
    thenCorrectMetaDataIsProvided();
}

TEST_F(AsyncJsonDataProvider, UpdateDataProvided)
{
    whenGetSpecificUpdateInformationRequestIssued();
    thenCorrectUpdateDataIsProvided();
}

} // namespace test
} // namespace data_provider
} // namespace detail
} // namespace info
} // namespace update
} // namespace nx
