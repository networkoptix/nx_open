#include <future>

#include <gtest/gtest.h>

#include <nx/network/cloud/cloud_module_url_fetcher.h>
#include <nx/network/cloud/connection_mediator_url_fetcher.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/future.h>
#include <nx/utils/scope_guard.h>

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
        auto fetcher = std::make_unique<cloud::CloudModuleUrlFetcher>(moduleName);
        auto fetcherGuard = makeScopeGuard([&fetcher]() { fetcher->pleaseStopSync(); });
        fetcher->setModulesXmlUrl(m_modulesUrl);

        nx::utils::promise<QUrl> urlFoundPromise;
        fetcher->get(
            [&urlFoundPromise](nx_http::StatusCode::Value statusCode, QUrl url)
            {
                if (statusCode != nx_http::StatusCode::ok)
                {
                    urlFoundPromise.set_exception(
                        std::make_exception_ptr(std::logic_error(
                            nx_http::StatusCode::toString(statusCode).toStdString())));
                }
                else
                {
                    urlFoundPromise.set_value(url);
                }
            });
     
        return urlFoundPromise.get_future().get();
    }

protected:
    TestHttpServer& httpServer()
    {
        return m_server;
    }

    void setModulesUrl(QUrl modulesUrl)
    {
        m_modulesUrl = modulesUrl;
        m_fetcher.setModulesXmlUrl(m_modulesUrl);
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

        ASSERT_TRUE(m_server.bindAndListen()) << SystemError::getLastOSErrorText().toStdString();

        setModulesUrl(
            QUrl(lit("http://%1%2").arg(m_server.serverAddress().toString()).arg(testPath)));
    }
};

TEST_F(CloudModuleUrlFetcher, common)
{
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("cdb"));
    ASSERT_EQ(QUrl("stuns://52.55.171.51:3345"), fetchModuleUrl("hpm"));
    ASSERT_EQ(QUrl("http://cloud-dev.hdw.mx:80"), fetchModuleUrl("notification_module"));
}

//-------------------------------------------------------------------------------------------------

static const char* modulesXmlWithEndpoints = R"xml(
    <sequence>
        <set resName="cdb" resValue="cloud-dev.hdw.mx:80"/>
        <set resName="hpm" resValue="52.55.171.51:3345"/>
        <set resName="notification_module" resValue="cloud-dev.hdw.mx:80"/>
    </sequence>
)xml";

class CloudModuleUrlFetcherCompatibility:
    public CloudModuleUrlFetcher
{
public:
    CloudModuleUrlFetcherCompatibility():
        CloudModuleUrlFetcher(modulesXmlWithEndpoints)
    {
    }
};

TEST_F(CloudModuleUrlFetcherCompatibility, correctly_parses_endpoints)
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

class CloudModuleUrlFetcherCompatibilityAutoSchemeSelection:
    public CloudModuleUrlFetcher
{
public:
    CloudModuleUrlFetcherCompatibilityAutoSchemeSelection():
        CloudModuleUrlFetcher(modulesXmlWithEndpointsCdbSsl)
    {
    }
};

TEST_F(CloudModuleUrlFetcherCompatibilityAutoSchemeSelection, https)
{
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("cdb"));
    ASSERT_EQ(QUrl("stun://52.55.171.51:3345"), fetchModuleUrl("hpm"));
    ASSERT_EQ(QUrl("https://cloud-dev.hdw.mx:443"), fetchModuleUrl("notification_module"));
}

//-------------------------------------------------------------------------------------------------

namespace {

class HanglingMsgBodySource:
    public nx_http::AbstractMsgBodySource
{
public:
    virtual nx_http::StringType mimeType() const override
    {
        return "text/plain";
    }

    virtual boost::optional<uint64_t> contentLength() const override
    {
        return 125;
    }

    virtual void readAsync(
        nx::utils::MoveOnlyFunc<
            void(SystemError::ErrorCode, nx_http::BufferType)
        > /*completionHandler*/) override
    {
    }
};

template<typename MessageBody>
class CustomMessageBodyProvider:
    public nx_http::AbstractHttpRequestHandler
{
public:
    CustomMessageBodyProvider()
    {
    }

    virtual void processRequest(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request /*request*/,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler) override
    {
        nx_http::RequestResult requestResult(nx_http::StatusCode::ok);
        requestResult.dataSource = std::make_unique<MessageBody>();
        completionHandler(std::move(requestResult));
    }
};

} // namespace

class FtCloudModuleUrlFetcher:
    public CloudModuleUrlFetcher
{
public:

protected:
    void givenServerThatSendsNoResponse()
    {
        httpServer().registerRequestProcessor<
            CustomMessageBodyProvider<HanglingMsgBodySource>>("/cloud_modules.xml");
        setModulesUrl(
            QUrl(lit("http://%1/cloud_modules.xml").arg(httpServer().serverAddress().toString())));
    }
    
    void assertUrlFetcherReportsError()
    {
        try
        {
            fetchModuleUrl("hpm");
            GTEST_FAIL();
        }
        catch (std::exception& /*e*/)
        {
        }
    }
};

TEST_F(FtCloudModuleUrlFetcher, no_response_from_cloud)
{
    givenServerThatSendsNoResponse();
    assertUrlFetcherReportsError();
}

TEST_F(FtCloudModuleUrlFetcher, cancellation)
{
    for (int i = 0; i < 7; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            cloud::CloudModuleUrlFetcher::ScopedOperation operation(&m_fetcher);

            std::string s;
            operation.get(
                [&s](
                    nx_http::StatusCode::Value /*resCode*/,
                    QUrl endpoint)
                {
                    //if called after destruction, will get segfault here
                    s = endpoint.toString().toStdString();
                });

            if (j == 0)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(
                    nx::utils::random::number<int>(1, 99)));
            }
        }

        m_fetcher.pleaseStopSync();
    }
}

//-------------------------------------------------------------------------------------------------

static const char* kSingleMediatorUrlXml = R"xml(
    <sequence>
        <set resName="cdb" resValue="https://cloud-test.hdw.mx:443"/>
        <set resName="hpm" resValue="stun://cloud-test.hdw.mx:3345"/>
        <set resName="notification_module" resValue="https://cloud-test.hdw.mx:443"/>
    </sequence>
)xml";

static const char* kMediatorHasDifferentAddressForTcpAndUdpXml = R"xml(
    <sequence>
        <set resName="cdb" resValue="https://cloud-test.hdw.mx:443"/>
        <sequence>
            <set resName="hpm.tcpUrl" resValue="http://cloud-test.hdw.mx:3345"/>
            <set resName="hpm.udpUrl" resValue="stun://cloud-test.hdw.mx:3346"/>
        </sequence>
        <set resName="notification_module" resValue="https://cloud-test.hdw.mx:443"/>
    </sequence>
)xml";

static const char* kConnectionMediatorXmlPath = "/mediator_test_modules.xml";

class MediatorUrlFetcher:
    public CloudModuleUrlFetcher
{
protected:
    void givenXmlWithSingleMediatorUrl()
    {
        httpServer().registerStaticProcessor(
            kConnectionMediatorXmlPath,
            kSingleMediatorUrlXml,
            "application/xml");
        m_expectedResult.statusCode = nx_http::StatusCode::ok;
        m_expectedResult.tcpUrl = "stun://cloud-test.hdw.mx:3345";
        m_expectedResult.udpUrl = "stun://cloud-test.hdw.mx:3345";
    }

    void givenXmlWithSeparateMediatorUrls()
    {
        httpServer().registerStaticProcessor(
            kConnectionMediatorXmlPath,
            kMediatorHasDifferentAddressForTcpAndUdpXml,
            "application/xml");
        m_expectedResult.statusCode = nx_http::StatusCode::ok;
        m_expectedResult.tcpUrl = "http://cloud-test.hdw.mx:3345";
        m_expectedResult.udpUrl = "stun://cloud-test.hdw.mx:3346";
    }

    void whenParseXml()
    {
        m_urlFetcher.setModulesXmlUrl(url::Builder().setScheme("http")
            .setEndpoint(httpServer().serverAddress()).setPath(kConnectionMediatorXmlPath));

        nx::utils::promise<void> done;
        m_urlFetcher.get(
            [this, &done](
                nx_http::StatusCode::Value statusCode, QUrl tcpUrl, QUrl udpUrl)
            {
                m_actualResult.statusCode = statusCode;
                m_actualResult.tcpUrl = tcpUrl;
                m_actualResult.udpUrl = udpUrl;
                done.set_value();
            });
        done.get_future().wait();
    }

    void thenUrlsAreFound()
    {
        ASSERT_EQ(m_expectedResult.statusCode, m_actualResult.statusCode);
        ASSERT_EQ(m_expectedResult.tcpUrl, m_actualResult.tcpUrl);
        ASSERT_EQ(m_expectedResult.udpUrl, m_actualResult.udpUrl);
    }

private:
    struct Result
    {
        nx_http::StatusCode::Value statusCode;
        QUrl tcpUrl;
        QUrl udpUrl;
    };

    Result m_expectedResult;
    Result m_actualResult;
    cloud::ConnectionMediatorUrlFetcher m_urlFetcher;
};

TEST_F(MediatorUrlFetcher, oldXmlIsAccepted)
{
    givenXmlWithSingleMediatorUrl();
    whenParseXml();
    thenUrlsAreFound();
}

TEST_F(MediatorUrlFetcher, xmlWithSeparateTcpAndUdpUrls)
{
    givenXmlWithSeparateMediatorUrls();
    whenParseXml();
    thenUrlsAreFound();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
