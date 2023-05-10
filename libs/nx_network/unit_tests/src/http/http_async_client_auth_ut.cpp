// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/url/url_builder.h>

#include "http_async_client_auth_ut.h"

namespace nx::network::http::test {

namespace {

static constexpr char kTestPath[] = "/HttpAsyncClient_auth";
static constexpr char kDefaultUsername[] = "zorz_user";
static constexpr char kDefaultPassword[] = "zorz_pass";

static constexpr char kDefaultBasicHeader[] = "WWW-Authenticate: Basic realm=\"ZORZ\"\r\n";
static constexpr char kResponseForDefaultBasicHeader[] = "Basic em9yel91c2VyOnpvcnpfcGFzcw==";

static constexpr char kDefaultDigestHeader[] =
    "WWW-Authenticate: Digest algorithm=\"MD5\", nonce=\"cUySLvm\", realm=\"VMS\"\r\n";

static constexpr char kResponseForDefaultDigestHeader[] =
    "Digest username=\"zorz_user\", realm=\"VMS\", nonce=\"cUySLvm\", "
    "uri=\"/HttpAsyncClient_auth\", response=\"4a5ec2fdc1d7dd43dd6fb345944583c5\", "
    "algorithm=\"MD5\"";

static constexpr char kLowercaseMd5DigestHeader[] =
    "WWW-Authenticate: Digest realm=\"Http Server\", "
    "nonce=\"f3164f6a1801ecb0870af2c468a6d7af\", algorithm=md5, qop=\"auth\"\r\n";

static constexpr char kResponseForLowercaseMd5DigestHeaderCommonAttrs[] =
    "Digest username=\"zorz_user\", realm=\"Http Server\", "
    "nonce=\"f3164f6a1801ecb0870af2c468a6d7af\", uri=\"/HttpAsyncClient_auth\", "
    "algorithm=\"md5\", qop=\"auth\"";

} // namespace

void AuthHttpServer::processConnection(AbstractStreamSocket* connection)
{
    connection->setRecvTimeout(kNoTimeout);
    connection->setSendTimeout(kNoTimeout);

    auto [rc, request] = readRequest(connection);
    while (rc > 0)
    {
        m_receivedRequests.push_back(std::move(request));
        sendResponse(connection);
        std::tie(rc, request) = readRequest(connection);
    }

    ASSERT_EQ(0, rc) << "System error: " << SystemError::getLastOSErrorText();
}


std::vector<nx::Buffer> AuthHttpServer::receivedRequests()
{
    return m_receivedRequests;
}

void AuthHttpServer::appendAuthHeader(AuthHeader value)
{
    ASSERT_NE(value.type, AuthType::authBasicAndDigest);
    m_authHeaders.push_back(std::move(value));
}

nx::Buffer AuthHttpServer::nextResponse()
{
    nx::Buffer response;
    switch (m_nextRepsonse)
    {
        case NextResponse::unauthorized:
            m_nextRepsonse = NextResponse::success;

            response =
                "HTTP/1.1 401 Unauthorized\r\n"
                "Content-Length: 0\r\n";
           for(const auto& authData: m_authHeaders)
               response.append(authData.header);
            response.append("\r\n");

            break;
        case NextResponse::success:
            m_nextRepsonse = NextResponse::unauthorized;
            response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Length: 0\r\n"
                "\r\n";
            break;
        default:
            NX_ASSERT(false);
    }

    NX_VERBOSE(this, "Sending response: %1", response);
    return response;
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

    ASSERT_EQ(response.size(), connection->send(response.data(), response.size()));
}


//-------------------------------------------------------------------------------------------------

HttpClientAsyncAuthorization::HttpClientAsyncAuthorization():
    m_httpClient(std::make_unique<http::AsyncClient>(ssl::kAcceptAnyCertificate))
{
    m_httpClient->setTimeouts(http::AsyncClient::kInfiniteTimeouts);
}

HttpClientAsyncAuthorization::~HttpClientAsyncAuthorization()
{
    m_httpClient->pleaseStopSync();
}

void HttpClientAsyncAuthorization::givenHttpServerWithAuthorization(
    std::vector<AuthHttpServer::AuthHeader> authData)
{
    m_httpServer = std::make_unique<AuthHttpServer>();
    ASSERT_TRUE(m_httpServer->bindAndListen(SocketAddress::anyPrivateAddress));

    for (auto &headerAndResponse: authData)
        m_httpServer->appendAuthHeader(std::move(headerAndResponse));
    m_httpServer->start();
}

void HttpClientAsyncAuthorization::whenClientSendHttpRequestAndIsRequiredToUse(
    AuthType auth, const char *username)
{
    m_httpClient->setAuthType(auth);
    if (username == nullptr)
        username = kDefaultUsername;

    const auto url = url::Builder()
        .setScheme(http::kUrlSchemeName)
        .setEndpoint(m_httpServer->endpoint())
        .setPath(kTestPath)
        .setUserName(username)
        .setPassword(kDefaultPassword)
        .toUrl();

    std::promise<void> done;
    m_httpClient->post(
        [this, url, &done]()
        {
            m_httpClient->doGet(
                url,
                [&done]()
                {
                    done.set_value();
                });
        });
    done.get_future().wait();
}

void HttpClientAsyncAuthorization::thenClientRequestContainsAuthAttrs(
    const char* expectedAttrs)
{
    const auto requests = m_httpServer->receivedRequests();
    ASSERT_EQ(2, requests.size());

    header::Authorization expected;
    ASSERT_TRUE(expected.parse(expectedAttrs));

    Request request;
    ASSERT_TRUE(request.parse(requests[1]));
    ASSERT_TRUE(request.headers.contains("Authorization"));
    header::Authorization actual;
    ASSERT_TRUE(actual.parse(request.headers.find("Authorization")->second));

    if (actual.authScheme == header::AuthScheme::basic)
    {
        ASSERT_EQ(*expected.basic, *actual.basic);
    }
    else if (actual.authScheme == header::AuthScheme::digest)
    {
        for (const auto& [k, v]: expected.digest->params)
        {
            ASSERT_TRUE(actual.digest->params.contains(k));
            ASSERT_EQ(v, actual.digest->params.find(k)->second);
        }
    }
}

void HttpClientAsyncAuthorization::thenClientGotResponseWithCode(int expectedHttpCode)
{
    ASSERT_EQ(m_httpClient->response()->statusLine.statusCode, expectedHttpCode);
}

void HttpClientAsyncAuthorization::thenLastRequestAuthorizedOnServerAsUser(
    const std::string &username)
{
    std::ostringstream usernameSubstring;
    usernameSubstring << "username=\"" << username << "\"";
    ASSERT_NE(nx::Buffer::npos, m_httpServer->receivedRequests().back().find(usernameSubstring.str()));
}

const AuthHttpServer::AuthHeader basic = {AuthType::authBasic, kDefaultBasicHeader};
const AuthHttpServer::AuthHeader digest = {AuthType::authDigest, kDefaultDigestHeader};
const auto basicResponse = kResponseForDefaultBasicHeader;
const auto digestResponse = kResponseForDefaultDigestHeader;

INSTANTIATE_TEST_SUITE_P(HttpClientAsyncAuthorizationInstance, HttpClientAsyncAuthorization,
    ::testing::Values(
        TestParams{{basic}, AuthType::authBasicAndDigest, /* expected */ basicResponse, 200},
        TestParams{{digest}, AuthType::authBasicAndDigest, /* expected */ digestResponse, 200},
        TestParams{{basic}, AuthType::authBasic, /* expected */ basicResponse, 200},
        TestParams{{digest}, AuthType::authDigest, /* expected */ digestResponse, 200},
        TestParams{{digest, basic}, AuthType::authBasic, /* expected */ basicResponse, 200},
        TestParams{{basic, digest}, AuthType::authDigest, /* expected */ digestResponse, 200},

        // Should always prefer digest to basic:
        TestParams{{basic, digest}, AuthType::authBasicAndDigest, /* expected */ digestResponse, 200},
        TestParams{{digest, basic}, AuthType::authBasicAndDigest, /* expected */ digestResponse, 200},

        // Is not allowed to auth:
        TestParams{{digest}, AuthType::authBasic, /* expected */ nullptr, 401},
        TestParams{{basic}, AuthType::authDigest, /* expected */ nullptr, 401}
    ));

TEST_P(HttpClientAsyncAuthorization, AuthSelection)
{
    TestParams params = GetParam();

    givenHttpServerWithAuthorization(std::move(params.serverAuthHeaders));
    whenClientSendHttpRequestAndIsRequiredToUse(params.clientRequiredToUseAuthType);
    thenClientGotResponseWithCode(params.expectedHttpCode);
}

TEST_F(HttpClientAsyncAuthorization, lowercaseAlgorithm)
{
    givenHttpServerWithAuthorization({{AuthType::authDigest, kLowercaseMd5DigestHeader}});

    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest);

    thenClientGotResponseWithCode(200);
    thenClientRequestContainsAuthAttrs(kResponseForLowercaseMd5DigestHeaderCommonAttrs);
}

TEST_F(HttpClientAsyncAuthorization, cached_authorization_of_a_different_user_is_not_used)
{
    givenHttpServerWithAuthorization({{AuthType::authDigest, kDefaultDigestHeader}});

    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest, "Vasya");
    thenClientGotResponseWithCode(200);
    thenLastRequestAuthorizedOnServerAsUser("Vasya");

    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest, "Petya");
    thenClientGotResponseWithCode(200);
    thenLastRequestAuthorizedOnServerAsUser("Petya");
}

} // namespace nx::network::http::test
