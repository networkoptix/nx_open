#include <future>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

static const char* testPath = "/tst";

static const char* modulesXmlWithUrls = R"xml(
    <sequence>
        <set resName="cdb" resValue="https://cloud-dev.hdw.mx:443"/>
        <set resName="hpm" resValue="stuns://52.55.171.51:3345"/>
        <set resName="notification_module" resValue="http://cloud-dev.hdw.mx:80"/>
    </sequence>
)xml";

class CloudModuleUrlFetcher:
    public ::testing::Test
{
public:
    CloudModuleUrlFetcher(const char* modulesXmlBody = nullptr):
        m_fetcher(std::make_unique<nx::network::cloud::RandomEndpointSelector>()),
        m_modulesXmlBody(modulesXmlBody ? modulesXmlBody : modulesXmlWithUrls)
    {
        init();
    }

    ~CloudModuleUrlFetcher()
    {
        m_fetcher.pleaseStopSync();
    }

protected:
    cloud::CloudDbUrlFetcher m_fetcher;

    QUrl fetchModuleUrl(const char* moduleName)
    {
        auto fetcher = std::make_unique<cloud::CloudModuleUrlFetcher>(
            moduleName,
            std::make_unique<nx::network::cloud::RandomEndpointSelector>());
        fetcher->setModulesXmlUrl(m_modulesUrl);

        nx::utils::promise<QUrl> urlFoundPromise;
        fetcher->get(
            [&urlFoundPromise](nx_http::StatusCode::Value /*statusCode*/, QUrl url)
            {
                urlFoundPromise.set_value(url);
            });
        auto url = urlFoundPromise.get_future().get();

        fetcher->pleaseStopSync();

        return url;
    }

private:
    TestHttpServer m_server;
    const QByteArray m_modulesXmlBody;
    QUrl m_modulesUrl;

    void init()
    {
        m_server.registerStaticProcessor(
            testPath,
            m_modulesXmlBody,
            "plain/text");
        ASSERT_TRUE(m_server.bindAndListen());

        m_modulesUrl =
            QUrl(lit("http://%1%2").arg(m_server.serverAddress().toString()).arg(testPath));

        m_fetcher.setModulesXmlUrl(m_modulesUrl);
    }
};

TEST_F(CloudModuleUrlFetcher, common)
{
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("cdb"));
    ASSERT_EQ(QUrl("stuns://52.55.171.51:3345"), fetchModuleUrl("hpm"));
    ASSERT_EQ(QUrl("http://cloud-dev.hdw.mx:80"), fetchModuleUrl("notification_module"));
}

class FtCloudModuleEndPointFetcher:
    public CloudModuleUrlFetcher
{
};

TEST_F(FtCloudModuleEndPointFetcher, cancellation)
{
    for (int i = 0; i < 100; ++i)
    {
        for (int j = 0; j < 10; ++j)
        {
            cloud::CloudModuleUrlFetcher::ScopedOperation operation(&m_fetcher);

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
                    nx::utils::random::number<int>(1, 99)));
        }

        m_fetcher.pleaseStopSync();
    }
}

//-------------------------------------------------------------------------------------------------

static const char* modulesXmlWithEndpoints = R"xml(
    <sequence>
        <set resName="cdb" resValue="cloud-dev.hdw.mx:80"/>
        <set resName="hpm" resValue="52.55.171.51:3345"/>
        <set resName="notification_module" resValue="cloud-dev.hdw.mx:80"/>
    </sequence>
)xml";

class CloudModuleEndPointFetcherCompatibility:
    public CloudModuleUrlFetcher
{
public:
    CloudModuleEndPointFetcherCompatibility():
        CloudModuleUrlFetcher(modulesXmlWithEndpoints)
    {
    }
};

TEST_F(CloudModuleEndPointFetcherCompatibility, correctly_parses_endpoints)
{
    ASSERT_EQ(QUrl("http://cloud-dev.hdw.mx:80"), fetchModuleUrl("cdb"));
    ASSERT_EQ(QUrl("stun://52.55.171.51:3345"), fetchModuleUrl("hpm"));
    ASSERT_EQ(QUrl("http://cloud-dev.hdw.mx:80"), fetchModuleUrl("notification_module"));
}

//-------------------------------------------------------------------------------------------------

static const char* modulesXmlWithEndpointsCdbSsl = R"xml(
    <sequence>
        <set resName="cdb" resValue="cloud-dev.hdw.mx:443"/>
        <set resName="hpm" resValue="52.55.171.51:3345"/>
        <set resName="notification_module" resValue="cloud-dev.hdw.mx:443"/>
    </sequence>
)xml";

class CloudModuleEndPointFetcherCompatibilityAutoSchemeSelection:
    public CloudModuleUrlFetcher
{
public:
    CloudModuleEndPointFetcherCompatibilityAutoSchemeSelection():
        CloudModuleUrlFetcher(modulesXmlWithEndpointsCdbSsl)
    {
    }
};

TEST_F(CloudModuleEndPointFetcherCompatibilityAutoSchemeSelection, https)
{
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("cdb"));
    ASSERT_EQ(QUrl("stun://52.55.171.51:3345"), fetchModuleUrl("hpm"));
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("notification_module"));
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
