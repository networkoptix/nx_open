#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/string.h>

namespace nx_http {
namespace test {

static const char* testPath = "/test/test";
static const char* testMessageBody = "test message body";

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
        ASSERT_TRUE(m_resourceServer->registerStaticProcessor(
            testPath, testMessageBody, "text/plain"));

        givenRedirectServer();
    }

    void givenTwoHttpServersWithRedirectionLoop()
    {
        givenResourceServer();
        givenRedirectServer();
        ASSERT_TRUE(m_resourceServer->registerRedirectHandler(testPath, m_redirectUrl));
    }

    void givenHttpResourceWithAuthentication()
    {
        givenResourceServer();
        ASSERT_TRUE(m_resourceServer->registerStaticProcessor(
            testPath, testMessageBody, "text/plain"));

        // Enabling authentication.
        const QString userName = nx::utils::generateRandomName(7);
        const QString password = nx::utils::generateRandomName(7);
        m_httpClient.setUserName(userName);
        m_httpClient.setUserPassword(password);

        m_resourceServer->setAuthenticationEnabled(true);
        m_resourceServer->registerUserCredentials(userName.toUtf8(), password.toUtf8());
    }

    void givenResourceServer()
    {
        ASSERT_TRUE(m_resourceServer->bindAndListen());
        m_actualUrl = QUrl(lm("http://%1%2")
            .arg(m_resourceServer->serverAddress().toString()).arg(testPath));
    }

    void givenRedirectServer()
    {
        ASSERT_TRUE(m_redirector->bindAndListen());
        m_redirectUrl = QUrl(lm("http://%1%2")
            .arg(m_redirector->serverAddress().toString()).arg(testPath));
        ASSERT_TRUE(m_redirector->registerRedirectHandler(testPath, m_actualUrl));
    }

    void whenRequestingRedirectedResource()
    {
        ASSERT_TRUE(m_httpClient.doGet(m_redirectUrl));
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
        ASSERT_EQ(QByteArray(testMessageBody), msgBody);

        ASSERT_EQ(m_redirectUrl, m_httpClient.url());
        ASSERT_EQ(m_actualUrl, m_httpClient.contentLocationUrl());
    }

    void thenClientShouldPerformFiniteNumberOfRedirectionAttempts()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(StatusCode::movedPermanently, m_httpClient.response()->statusLine.statusCode);
    }

private:
    std::unique_ptr<TestHttpServer> m_redirector;
    std::unique_ptr<TestHttpServer> m_resourceServer;
    nx_http::HttpClient m_httpClient;
    QUrl m_redirectUrl;
    QUrl m_actualUrl;
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

} // namespace test
} // namespace nx_http
