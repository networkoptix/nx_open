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

constexpr const int socketTimeout = 1000;

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
    connection->setRecvTimeout(socketTimeout);
    connection->setSendTimeout(socketTimeout);

    int rc;
    nx::Buffer request;
    std::tie(rc, request) = readRequest(connection);
    while (rc > 0)
    {
        m_receivedRequests.emplace_back(std::move(request));
        sendResponse(connection);
        std::tie(rc, request) = readRequest(connection);
    }

    ASSERT_EQ(0, rc) << "System error: " << SystemError::getLastOSErrorText().toStdString();
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
    nx::Buffer unauthorizedResponse =
        "HTTP/1.1 401 Unauthorized\r\n"
        "Content-Length: 0\r\n" +
        m_firstAuthHeader +
        m_secondAuthHeader +
        "\r\n";

    const nx::Buffer successResponse =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 0\r\n"
        "\r\n";

    switch (m_nextRepsonse) {
        case NextResponse::unauthorized:
            m_nextRepsonse = NextResponse::success;
            return unauthorizedResponse;
        case NextResponse::success:
            m_nextRepsonse = NextResponse::unauthorized;
            return successResponse;
        default:
            NX_ASSERT(false);
    }

    return nx::Buffer(); // To avoid compilation warning.
}

std::pair<int, nx::Buffer> AuthHttpServer::readRequest(AbstractStreamSocket* connection)
{
    nx::Buffer buffer;
    buffer.resize(4096);

    int rc = connection->recv(buffer.data(), buffer.size());
    if (rc > 0)
        buffer.resize(rc);
    return {rc, buffer};
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

void HttpClientAsyncAuthorization2::thenClientAuthenticatedBy(std::optional<AuthType> exptecedAuth)
{
    const auto requests = m_httpServer->receivedRequests();

    // No auth exptected.
    if (!exptecedAuth) {
        ASSERT_EQ(requests.size(), 1);
        return;
    }

    ASSERT_EQ(requests.size(), 2);
    switch (*exptecedAuth) {
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

void HttpClientAsyncAuthorization2::thenClientGotResponseWithCode(int expectedHttpCode)
{
    ASSERT_EQ(m_httpClient->response()->statusLine.statusCode, expectedHttpCode);
}

constexpr auto basic = AuthType::authBasic;
constexpr auto digest = AuthType::authDigest;
constexpr auto both = AuthType::authBasicAndDigest;

INSTANTIATE_TEST_CASE_P(HttpClientAsyncAuthorizationInstance, HttpClientAsyncAuthorization2,
    ::testing::Values(
        TestParams{basic, std::nullopt, both, /* expected */ basic, 200},
        TestParams{digest, std::nullopt, both, /* expected */ digest, 200},
        TestParams{basic, std::nullopt, basic, /* expected */ basic, 200},
        TestParams{digest, std::nullopt, digest, /* expected */ digest, 200},
        TestParams{digest, basic, basic, /* expected */ basic, 200},
        TestParams{basic, digest, digest, /* expected */ digest, 200},

        // Should always prefer digest to basic:
        TestParams{basic, digest, both, /* expected */ digest, 200},
        TestParams{digest, basic, both, /* expected */ digest, 200},

        // Is not allowed to auth:
        TestParams{digest, std::nullopt, basic, /* expected */ std::nullopt, 401},
        TestParams{basic, std::nullopt, digest, /* expected */ std::nullopt, 401}
    ));

TEST_P(HttpClientAsyncAuthorization2, AuthSelection)
{
    TestParams params = GetParam();
    givenHttpServerWithAuthorization(params.serverAuthTypeFirst, params.serverAuthTypeSecond);
    whenClientSendHttpRequestAndIsRequiredToUse(params.clientRequiredToUseAuthType);
    thenClientAuthenticatedBy(params.expectedAuthType);
    thenClientGotResponseWithCode(params.expectedHttpCode);
}

} // namespace test
} // namespace nx
} // namespace network
} // namespace http
