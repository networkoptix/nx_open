// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <QUrlQuery>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/ssl/context.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

namespace nx::network::http::test {

static constexpr char kTestPath[] = "/test/test";
static constexpr char kContentServerPathPrefix[] = "/AsyncHttpClientRedirectTest/";
static constexpr char kTestMessageBody[] = "test message body";

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
            nx::network::url::joinPath(kContentServerPathPrefix, kTestPath),
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
        m_actualUrl = nx::utils::Url(nx::format("http://localhost:%1%2")
            .arg(m_resourceServer->serverAddress().port)
            .arg(nx::network::url::joinPath(kContentServerPathPrefix, kTestPath)));
    }

    void givenHttpsResourceServer()
    {
        m_resourceServer.reset();

        m_resourceServer = std::make_unique<TestHttpServer>(
            server::Role::resourceServer,
            SocketFactory::createSslAdapter(
                std::make_unique<TCPServerSocket>(AF_INET),
                ssl::Context::instance(),
                ssl::EncryptionUse::always));

        registerContentHandlers();
        givenResourceServer();

        m_actualUrl.setScheme(http::kSecureUrlSchemeName);
    }

    void givenRedirectServer()
    {
        ASSERT_TRUE(m_redirector->bindAndListen());
        m_redirectUrl = nx::utils::Url(nx::format("http://127.0.0.1:%1%2")
            .arg(m_redirector->serverAddress().port).arg(kTestPath));
        ASSERT_TRUE(m_redirector->registerRedirectHandler(kTestPath, m_actualUrl));
    }

    void enableAuthenticationOnContentServer()
    {
        const auto credentials = nx::network::http::PasswordCredentials(
            nx::utils::generateRandomName(7), nx::utils::generateRandomName(7));
        m_httpClient.setCredentials(credentials);

        m_resourceServer->enableAuthentication(".*");
        m_resourceServer->registerUserCredentials(credentials);
    }

    void whenRequestingRedirectedResource()
    {
        ASSERT_TRUE(m_httpClient.doGet(m_redirectUrl));
    }

    void whenPostingResourceToRedirectionServer(std::map<std::string, std::string> params = {})
    {
        QUrlQuery query;
        for (const auto& param: params)
            query.addQueryItem(QString::fromStdString(param.first), QString::fromStdString(param.second));

        auto url = m_redirectUrl;
        url.setQuery(query.query());

        m_httpClient.addAdditionalHeader("Nx-Additional-Header", "value");
        m_httpClient.doPost(url, "text/plain", kTestMessageBody);
    }

    void thenClientFetchedResourceFromActualLocation()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
        ASSERT_EQ(
            "text/plain",
            nx::network::http::getHeaderValue(m_httpClient.response()->headers, "Content-Type"));
        nx::Buffer msgBody;
        while (!m_httpClient.eof())
            msgBody += m_httpClient.fetchMessageBodyBuffer();
        ASSERT_EQ(kTestMessageBody, msgBody);

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
        ASSERT_EQ(
            hostHeader->second,
            nx::format("%1:%2").args(m_actualUrl.host(), m_actualUrl.port()).toStdString());
    }

    void andDigestUrlIsCorrectAndFullyEncoded()
    {
        const auto authHeader =
            m_prevReceivedPostRequest->headers.find(nx::network::http::header::Authorization::NAME);
        ASSERT_NE(authHeader, m_prevReceivedPostRequest->headers.end());
        nx::network::http::header::Authorization auth(nx::network::http::header::AuthScheme::digest);
        ASSERT_TRUE(auth.parse(authHeader->second));
        ASSERT_EQ(auth.digest->params["uri"],
            m_prevReceivedPostRequest->requestLine.url.toStdString(QUrl::FullyEncoded));
    }

    void registerAdditionalRequestHeader(const std::string& name, const std::string& value)
    {
        m_httpClient.addAdditionalHeader(name, value);
    }

private:
    HttpClient m_httpClient{ssl::kAcceptAnyCertificate};
    std::unique_ptr<TestHttpServer> m_redirector;
    std::unique_ptr<TestHttpServer> m_resourceServer;
    nx::utils::Url m_redirectUrl;
    nx::utils::Url m_actualUrl;
    nx::utils::SyncQueue<nx::network::http::Request> m_postResourceRequests;
    std::optional<nx::network::http::Request> m_prevReceivedPostRequest;

    void savePostedResource(
        nx::network::http::RequestContext requestContext,
        nx::network::http::RequestProcessedHandler completionHandler)
    {
        m_postResourceRequests.push(std::move(requestContext.request));
        completionHandler(nx::network::http::StatusCode::ok);
    }

    void registerContentHandlers()
    {
        using namespace std::placeholders;

        ASSERT_TRUE(m_resourceServer->registerStaticProcessor(
            nx::network::url::joinPath(kContentServerPathPrefix, kTestPath),
            kTestMessageBody,
            "text/plain",
            nx::network::http::Method::get));

        ASSERT_TRUE(m_resourceServer->registerRequestProcessorFunc(
            nx::network::url::joinPath(kContentServerPathPrefix, kTestPath),
            std::bind(&AsyncHttpClientRedirect::savePostedResource, this, _1, _2),
            nx::network::http::Method::post));
    }
};

TEST_F(AsyncHttpClientRedirect, simple_redirect_by_301_and_location)
{
    givenTwoHttpServersWithRedirection();
    whenRequestingRedirectedResource();
    thenClientFetchedResourceFromActualLocation();
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
    thenClientFetchedResourceFromActualLocation();
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

TEST_F(AsyncHttpClientRedirect, redirect_from_http_to_https)
{
    givenHttpsResourceServer();
    givenRedirectServer();

    whenRequestingRedirectedResource();

    thenClientFetchedResourceFromActualLocation();
}

} // namespace nx::network::http::test
