#include <gtest/gtest.h>
#include <nx/mediaserver/rest/updates2/updates2_info_rest_handler.h>
#include <nx/mediaserver/rest/updates2/detail/update_request_data_factory.h>
#include <nx/update/info/detail/data_provider/raw_data_provider_factory.h>
#include <nx/update/info/detail/data_provider/test_support/impl/async_json_provider_mockup.h>
#include <nx/update/info/update_request_data.h>
#include <test_support/mediaserver_launcher.h>
#include <nx/network/http/http_client.h>
#include <nx/api/updates2/available_update_info_data.h>

namespace nx {
namespace mediaserver {
namespace rest {
namespace updates2 {
namespace test {

using namespace update::info::detail::data_provider;
static QString urlPattern = "http://admin:admin@127.0.0.1/api/updates2";

static const QString kDefaultCustomization = "default";
static const QString kNonExistentCustomization = "non-existent";
static const QString kDefaultCloudHost = "nxvms.com";
static const QnSoftwareVersion kDefaultVersion = QnSoftwareVersion(2, 0);

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
        detail::UpdateRequestDataFactory::setFactoryFunc(nullptr);
    }

    static void whenRequestDataMockedUpWith(
        const QString& cloudHost,
        const QString& customization,
        const QnSoftwareVersion& version)
    {
        detail::UpdateRequestDataFactory::setFactoryFunc(
            [=]()
            {
                return update::info::UpdateRequestData(cloudHost, customization, version);
            });
    }

    void whenGetRequestIssuedAndResponseReveived()
    {
        nx::utils::Url getUrl(urlPattern);
        getUrl.setPort(m_mediaServerLauncher->port());
        m_requestResult = m_httpClient.doGet(getUrl);
    }

    void andWhenResponseBodyParsed()
    {
        const auto optionalBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(static_cast<bool>(optionalBody));
        ASSERT_FALSE(optionalBody->isEmpty());
        parseBody(*optionalBody);
    }

    void thenItShouldBeSuccessfull() const
    {
        ASSERT_TRUE(m_requestResult);
        const nx_http::Response* response = m_httpClient.response();
        ASSERT_TRUE(response);
        ASSERT_EQ(nx_http::StatusCode::ok ,response->statusLine.statusCode);
    }

    void thenContentShouldHaveVersion(const QnSoftwareVersion& version) const
    {
        ASSERT_EQ(version, QnSoftwareVersion(m_responseUpdatesInfo.version));
    }
private:
    std::unique_ptr<MediaServerLauncher> m_mediaServerLauncher = nullptr;
    nx_http::HttpClient m_httpClient;
    bool m_requestResult = false;
    api::AvailableUpdatesInfo m_responseUpdatesInfo;

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

    void parseBody(const QByteArray& rawData)
    {
        QnJsonRestResult restResult;
        ASSERT_TRUE(QJson::deserialize(rawData, &restResult));
        ASSERT_EQ(QnRestResult::Error::NoError, restResult.error);
        ASSERT_TRUE(
            QJson::deserialize<api::AvailableUpdatesInfo>(
                restResult.reply,
                &m_responseUpdatesInfo));
    }
};

TEST_F(Updates2RestHandler, checkResults_Ok)
{
    whenRequestDataMockedUpWith(kDefaultCloudHost, kDefaultCustomization, kDefaultVersion);
    whenGetRequestIssuedAndResponseReveived();
    thenItShouldBeSuccessfull();
    andWhenResponseBodyParsed();
    thenContentShouldHaveVersion(QnSoftwareVersion(3, 1, 0, 16975));
}

TEST_F(Updates2RestHandler, checkResults_Null_WrongCustomization)
{
    whenRequestDataMockedUpWith(kDefaultCloudHost, kNonExistentCustomization, kDefaultVersion);
    whenGetRequestIssuedAndResponseReveived();
    thenItShouldBeSuccessfull();
    andWhenResponseBodyParsed();
    thenContentShouldHaveVersion(QnSoftwareVersion());
}

} // namespace test
} // namespace updates2
} // namespace rest
} // namespace mediaserver
} // namespace nx