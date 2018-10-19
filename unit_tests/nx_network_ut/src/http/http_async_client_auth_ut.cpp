#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include <nx/network/url/url_builder.h>

#include "http_async_client_auth_ut.h"

namespace nx {
namespace network {
namespace http {
namespace test {

namespace {

const char* kTestPath = "/HttpAsyncClient_auth";
const char* kDefaultUsername = "zorz_user";
const char* kDefaultPassword = "zorz_pass";

const char* defaultBasicHeader = "WWW-Authenticate: Basic realm=\"Http Server\"\r\n";
const char* defaultDigestHeader =
    "WWW-Authenticate: Digest algorithm=\"MD5\", nonce=\"cUySLvm\", realm=\"VMS\"\r\n";

const char* resonseForDefaultBasicHeader = "Basic em9yel91c2VyOnpvcnpfcGFzcw==";
const char* resonseForDefaultDigestHeader =
        "Digest username=\"zorz_user\", realm=\"VMS\", nonce=\"cUySLvm\", "
        "uri=\"/HttpAsyncClient_auth\", response=\"4a5ec2fdc1d7dd43dd6fb345944583c5\", "
        "algorithm=\"MD5\"";

// TODO: check why it does not work and write test
//    static const char* digestHeader =
//        "WWW-Authenticate: Digest realm=\"Http Server\", "
//        "nonce=\"f3164f6a1801ecb0870af2c468a6d7af\", algorithm=md5, qop=\"auth\"\r\n"

} // namespace

void AuthHttpServer::processConnection(AbstractStreamSocket* connection)
{
    // TODO: Add while loop.
    auto request = readRequest(connection);
    m_receivedRequests.emplace_back(std::move(request));
    sendResponse(connection);

    request = readRequest(connection);
    m_receivedRequests.emplace_back(std::move(request));
    sendResponse(connection);
}

std::vector<nx::Buffer> AuthHttpServer::receivedRequests()
{
    return m_receivedRequests;
}

void AuthHttpServer::enableAuthHeaders(std::optional<AuthType> first, std::optional<AuthType> second)
{
    auto setHeader =
        [](nx::Buffer* const headerBuffer, std::optional<AuthType> headerType)
        {
            if (headerType.has_value())
            {
                switch (*headerType) {
                case AuthType::authBasic:
                    *headerBuffer = defaultBasicHeader;
                    break;
                case AuthType::authDigest:
                    *headerBuffer = defaultDigestHeader;
                    break;
                default:
                    NX_ASSERT(false);
                }
            }
            else
            {
                headerBuffer->clear();
            }
        };

    setHeader(&m_firstAuthHeader, first);
    setHeader(&m_secondAuthHeader, second);
}

nx::Buffer AuthHttpServer::nextResponse()
{
    static NextResponse nextRepsonse = NextResponse::unauthorized;

    nx::Buffer unauthorizedResponse =
        "HTTP/1.1 401 Unauthorized\r\n" +
        m_firstAuthHeader +
        m_secondAuthHeader +
        "\r\n";

    const nx::Buffer successResponse =
        "HTTP/1.1 200 OK\r\n"
        "\r\n";

    switch (nextRepsonse) {
        case NextResponse::unauthorized:
            nextRepsonse = NextResponse::success;
            return unauthorizedResponse;
        case NextResponse::success:
            nextRepsonse = NextResponse::unauthorized;
            return successResponse;
        default:
            NX_ASSERT(false);
    }

    return nx::Buffer(); // To avoid compilation warning.
}

nx::Buffer AuthHttpServer::readRequest(AbstractStreamSocket* connection)
{
    nx::Buffer buffer;
    buffer.resize(4096);

    int rc = connection->recv(buffer.data(), buffer.size());
    NX_ASSERT(rc > 0);  // Can't use ASSERT_EQ here.

    buffer.resize(rc);
    return buffer;
}

void AuthHttpServer::sendResponse(AbstractStreamSocket* connection)
{
    nx::Buffer response = nextResponse();

    ASSERT_EQ(response.size(), connection->send(response.constData(), response.size()));
}


//-------------------------------------------------------------------------------------------------
HttpClientAsyncAuthorization2::HttpClientAsyncAuthorization2():
    m_httpClient(std::make_unique<http::AsyncClient>())
{
}

HttpClientAsyncAuthorization2::~HttpClientAsyncAuthorization2()
{
    m_httpClient->pleaseStopSync();
}

void HttpClientAsyncAuthorization2::givenHttpServerWithAuthorization(std::optional<AuthType> first,
                                                                     std::optional<AuthType> second)
{
    m_httpServer = std::make_unique<AuthHttpServer>();
    ASSERT_TRUE(m_httpServer->bindAndListen(SocketAddress::anyPrivateAddress));
    m_httpServer->enableAuthHeaders(first, second);
    m_httpServer->start();
}


void HttpClientAsyncAuthorization2::whenClientSendHttpRequestAndIsRequiredToUse(AuthType auth)
{
    NX_ASSERT(auth == AuthType::authBasic || auth == AuthType::authDigest);
    m_httpClient->setAuthType(auth);

    const auto url = url::Builder()
        .setScheme(http::kUrlSchemeName)
        .setEndpoint(m_httpServer->endpoint())
        .setPath(kTestPath)
        .setUserName(kDefaultUsername)
        .setPassword(kDefaultPassword)
        .toUrl();

    std::promise<void> done;
    m_httpClient->post(
        [this, url, &done]()
        {
            m_httpClient->doGet(url, [&done]() { done.set_value(); });
        });
    done.get_future().wait();
}

void HttpClientAsyncAuthorization2::thenClientAuthenticatedBy(AuthType auth)
{
    static_cast<void>(auth);

    const auto requests = m_httpServer->receivedRequests();
    ASSERT_GE(requests.size(), 2);

    switch (auth) {
    case AuthType::authBasic:
        ASSERT_THAT(requests[1].toStdString(), testing::HasSubstr(resonseForDefaultBasicHeader));
        break;
    case AuthType::authDigest:
        ASSERT_THAT(requests[1].toStdString(), testing::HasSubstr(resonseForDefaultDigestHeader));
        break;
    case AuthType::authBasicAndDigest:
        ASSERT_THAT(requests[1].toStdString(), testing::AnyOf(
            testing::HasSubstr(resonseForDefaultDigestHeader),
            testing::HasSubstr(resonseForDefaultBasicHeader)));
        break;
    default:
        ASSERT_TRUE(false);
    }
}


// TODO: parametrize!
TEST_F(HttpClientAsyncAuthorization2, basicAuth)
{
    givenHttpServerWithAuthorization(AuthType::authBasic, std::nullopt);
    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authBasic);
    thenClientAuthenticatedBy(AuthType::authBasic);
}

TEST_F(HttpClientAsyncAuthorization2, digestAuth)
{
    givenHttpServerWithAuthorization(AuthType::authDigest, std::nullopt);
    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest);
    thenClientAuthenticatedBy(AuthType::authDigest);
}

TEST_F(HttpClientAsyncAuthorization2, digestBeforeBasicAuth)
{
    givenHttpServerWithAuthorization(AuthType::authDigest, AuthType::authBasic);
    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authBasic);
    thenClientAuthenticatedBy(AuthType::authBasic);
}

TEST_F(HttpClientAsyncAuthorization2, basicBeforeDigestAuth)
{
    givenHttpServerWithAuthorization(AuthType::authBasic, AuthType::authDigest);
    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest);
    thenClientAuthenticatedBy(AuthType::authDigest);
}

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
