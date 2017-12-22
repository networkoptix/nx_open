#include <gtest/gtest.h>
#include <nx/mediaserver/rest/updates2_rest_handler.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/test_support/impl/async_json_provider_mockup.h>
#include <nx/update/info/update_request_data.h>
#include <test_support/mediaserver_launcher.h>
#include <nx/network/http/http_client.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace test {

using namespace update::info::detail::data_provider;
static QString urlPattern = "http://admin:admin@127.0.0.1/api/updates2";

class Updates2RestHandler: public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        setUpMediaServer();
        mockupUpdateProvider();
    }

    virtual void TearDown() override
    {
        RawDataProviderFactory::setFactoryFunction(nullptr);
        UpdateRequestDataFactory::setFactoryFunc(nullptr);
    }

    static void whenRequestDataMockedUpWith(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& version)
    {
        UpdateRequestDataFactory::setFactoryFunc(
            [=]()
            {
                return update::info::UpdateRequestData(cloudHost, customization, version);
            });
    }

    void whenGetRequestIssued()
    {
        nx::utils::Url getUrl(urlPattern);
        getUrl.setPort(m_mediaServerLauncher->port());
        m_requestResult = m_httpClient.doGet(getUrl);
    }

    void thenItShouldBeSuccessfull() const
    {
        ASSERT_TRUE(m_requestResult);
        const nx_http::Response* response = m_httpClient.response();
        ASSERT_EQ(nx_http::StatusCode::ok ,response->statusLine.statusCode);
        ASSERT_FALSE(response->messageBody.isEmpty());
    }
private:
    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher = nullptr;
    nx_http::HttpClient m_httpClient;
    bool m_requestResult = false;

    void setUpMediaServer()
    {
        m_mediaServerLauncher.reset(new MediaServerLauncher());
        ASSERT_TRUE(m_mediaServerLauncher->start());
    }

    static void mockupUpdateProvider()
    {
        RawDataProviderFactory::setFactoryFunction(
            [](const QString& s, AbstractAsyncRawDataProviderHandler* handler)
            {
                return std::make_unique<test_support::AsyncJsonProviderMockup>(handler);
            });
    }
};

TEST_F(Updates2RestHandler, updateRequestProcessedByServer)
{
    whenGetRequestIssued();
    thenItShouldBeSuccessfull();
}

} // namespace test
} // namespace rest
} // namespace mediaserver
} // namespace nx