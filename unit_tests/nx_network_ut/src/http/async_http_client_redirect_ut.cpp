#include <gtest/gtest.h>

#include <QUrlQuery>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx_http {
namespace test {

static const nx::String kTestPath = "/test/test";
static const nx::String kContentServerPathPrefix = "/AsyncHttpClientRedirectTest/";
static const nx::String kTestMessageBody = "test message body";

class AsyncHttpClientRedirect:
    public ::testing::Test
{
public:
    AsyncHttpClientRedirect():
        m_redirector(std::make_unique<TestHttpServer>()),
        m_resourceServer(std::make_unique<TestHttpServer>())
    {
    }

protected:
    void givenTwoHttpServersWithRedirection()
    {
        givenResourceServer();
        registerContentHandlers();

        givenRedirectServer();
    }

    void givenTwoHttpServersWithRedirectionLoop()
    {
        givenResourceServer();
        givenRedirectServer();
        ASSERT_TRUE(m_resourceServer->registerRedirectHandler(
            nx::network::url::normalizePath(kContentServerPathPrefix + kTestPath),
            m_redirectUrl));
    }

    void givenHttpResourceWithAuthentication()
    {
        givenResourceServer();
        registerContentHandlers();

        enableAuthenticationOnContentServer();
    }

    void givenResourceServer()
    {
        ASSERT_TRUE(m_resourceServer->bindAndListen());
        m_actualUrl = QUrl(lm("http://localhost:%1%2")
            .arg(m_resourceServer->serverAddress().port)
            .arg(nx::network::url::normalizePath(kContentServerPathPrefix + kTestPath)));
    }

    void givenRedirectServer()
    {
        ASSERT_TRUE(m_redirector->bindAndListen());
        m_redirectUrl = QUrl(lm("http://127.0.0.1:%1%2")
            .arg(m_redirector->serverAddress().port).arg(kTestPath));
        ASSERT_TRUE(m_redirector->registerRedirectHandler(kTestPath, m_actualUrl));
    }

    void enableAuthenticationOnContentServer()
    {
        const QString userName = nx::utils::generateRandomName(7);
        const QString password = nx::utils::generateRandomName(7);
        m_httpClient.setUserName(userName);
        m_httpClient.setUserPassword(password);

        m_resourceServer->setAuthenticationEnabled(true);
        m_resourceServer->registerUserCredentials(userName.toUtf8(), password.toUtf8());
    }

    void whenRequestingRedirectedResource()
    {
        ASSERT_TRUE(m_httpClient.doGet(m_redirectUrl));
    }

    void whenPostingResourceToRedirectionServer(std::map<QString, QString> params = {})
    {
        QUrlQuery query;
        for (const auto& param: params)
            query.addQueryItem(param.first, param.second);

        auto url = m_redirectUrl;
        url.setQuery(query.query());

        m_httpClient.addAdditionalHeader("Nx-Additional-Header", "value");
        m_httpClient.doPost(url, "text/plain", kTestMessageBody);
    }

    void thenClientShouldFetchResourceFromActualLocation()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(
            "text/plain",
            nx_http::getHeaderValue(m_httpClient.response()->headers, "Content-Type"));
        QByteArray msgBody;
        while (!m_httpClient.eof())
            msgBody += m_httpClient.fetchMessageBodyBuffer();
        ASSERT_EQ(QByteArray(kTestMessageBody), msgBody);

        ASSERT_EQ(m_redirectUrl, m_httpClient.url());
        ASSERT_EQ(
            m_actualUrl.toString(),
            m_httpClient.contentLocationUrl().toString(QUrl::RemoveUserInfo));
    }

    void thenClientShouldPerformFiniteNumberOfRedirectionAttempts()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(StatusCode::movedPermanently, m_httpClient.response()->statusLine.statusCode);
    }

    void thenResourceFinallyPostedToContentServer()
    {
        ASSERT_EQ(kTestMessageBody, m_postResourceRequests.pop().messageBody);
    }

    void thenPostRequestIsReceived()
    {
        m_prevReceivedPostRequest = m_postResourceRequests.pop();
    }

    void andPostRequestToContentServerDoesNotContainDuplicateHeaders()
    {
        for (const auto& header: m_prevReceivedPostRequest->headers)
        {
            ASSERT_EQ(1U, m_prevReceivedPostRequest->headers.count(header.first))
                << header.first.data();
        }
    }

    void andHostHeaderHoldsCorrectValue()
    {
        const auto hostHeader = m_prevReceivedPostRequest->headers.find("Host");
        ASSERT_NE(hostHeader, m_prevReceivedPostRequest->headers.end());
        ASSERT_EQ(hostHeader->second, lm("%1:%2").args(m_actualUrl.host(),m_actualUrl.port()));
    }

    void andDigestUrlIsCorrectAndFullyEncoded()
    {
        const auto authHeader = 
            m_prevReceivedPostRequest->headers.find(nx_http::header::Authorization::NAME);
        ASSERT_NE(authHeader, m_prevReceivedPostRequest->headers.end());
        nx_http::header::Authorization auth(nx_http::header::AuthScheme::digest);
        ASSERT_TRUE(auth.parse(authHeader->second));
        ASSERT_EQ(auth.digest->params["uri"],
            m_prevReceivedPostRequest->requestLine.url.toString(QUrl::FullyEncoded).toUtf8());
    }

    void registerAdditionalRequestHeader(const nx::String& name, const nx::String& value)
    {
        m_httpClient.addAdditionalHeader(name, value);
    }

private:
    std::unique_ptr<TestHttpServer> m_redirector;
    std::unique_ptr<TestHttpServer> m_resourceServer;
    nx_http::HttpClient m_httpClient;
    QUrl m_redirectUrl;
    QUrl m_actualUrl;
    nx::utils::SyncQueue<nx_http::Request> m_postResourceRequests;
    boost::optional<nx_http::Request> m_prevReceivedPostRequest;

    void savePostedResource(
        nx_http::HttpServerConnection* const /*connection*/,
        nx::utils::stree::ResourceContainer /*authInfo*/,
        nx_http::Request request,
        nx_http::Response* const /*response*/,
        nx_http::RequestProcessedHandler completionHandler)
    {
        m_postResourceRequests.push(std::move(request));
        completionHandler(nx_http::StatusCode::ok);
    }

    void registerContentHandlers()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_resourceServer->registerStaticProcessor(
            nx::network::url::normalizePath(kContentServerPathPrefix + kTestPath),
            kTestMessageBody,
            "text/plain",
            nx_http::Method::get));

        ASSERT_TRUE(m_resourceServer->registerRequestProcessorFunc(
            nx::network::url::normalizePath(kContentServerPathPrefix + kTestPath),
            std::bind(&AsyncHttpClientRedirect::savePostedResource, this, _1, _2, _3, _4, _5),
            nx_http::Method::post));
    }
};

TEST_F(AsyncHttpClientRedirect, simple_redirect_by_301_and_location)
{
    givenTwoHttpServersWithRedirection();
    whenRequestingRedirectedResource();
    thenClientShouldFetchResourceFromActualLocation();
}

TEST_F(AsyncHttpClientRedirect, no_infinite_redirect_loop)
{
    givenTwoHttpServersWithRedirectionLoop();
    whenRequestingRedirectedResource();
    thenClientShouldPerformFiniteNumberOfRedirectionAttempts();
}

TEST_F(AsyncHttpClientRedirect, redirect_of_authorized_resource)
{
    givenHttpResourceWithAuthentication();
    givenRedirectServer();
    whenRequestingRedirectedResource();
    thenClientShouldFetchResourceFromActualLocation();
}

TEST_F(AsyncHttpClientRedirect, message_body_is_redirected)
{
    givenTwoHttpServersWithRedirection();
    whenPostingResourceToRedirectionServer();
    thenResourceFinallyPostedToContentServer();
}

TEST_F(AsyncHttpClientRedirect, no_duplicate_headers_in_redirected_request)
{
    registerAdditionalRequestHeader("TestHeader", "Value");

    givenTwoHttpServersWithRedirection();
    whenPostingResourceToRedirectionServer();

    thenPostRequestIsReceived();
    andPostRequestToContentServerDoesNotContainDuplicateHeaders();
}

TEST_F(AsyncHttpClientRedirect, digest_url_is_correct)
{
    givenTwoHttpServersWithRedirection();
    enableAuthenticationOnContentServer();

    whenPostingResourceToRedirectionServer({{"param", "value"}});

    thenPostRequestIsReceived();
    andHostHeaderHoldsCorrectValue();
    andDigestUrlIsCorrectAndFullyEncoded();
}

} // namespace test
} // namespace nx_http
