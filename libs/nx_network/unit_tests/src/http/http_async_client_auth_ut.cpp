#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gmock/gmock-matchers.h>

#include <nx/network/url/url_builder.h>

#include "http_async_client_auth_ut.h"

namespace nx::network::http::test {

namespace {

constexpr const int socketTimeout = 1000;

const char* kTestPath = "/HttpAsyncClient_auth";
const char* kDefaultUsername = "zorz_user";
const char* kDefaultPassword = "zorz_pass";

const char* kDefaultBasicHeader = "WWW-Authenticate: Basic realm=\"ZORZ\"\r\n";
const char* kDefaultDigestHeader =
    "WWW-Authenticate: Digest algorithm=\"MD5\", nonce=\"cUySLvm\", realm=\"VMS\"\r\n";
const char* kLowercaseMd5DigestHeader =
    "WWW-Authenticate: Digest realm=\"Http Server\", "
    "nonce=\"f3164f6a1801ecb0870af2c468a6d7af\", algorithm=md5, qop=\"auth\"\r\n";

const char* kResponseForDefaultBasicHeader = "Basic em9yel91c2VyOnpvcnpfcGFzcw==";
const char* kResponseForDefaultDigestHeader =
        "Digest username=\"zorz_user\", realm=\"VMS\", nonce=\"cUySLvm\", "
        "uri=\"/HttpAsyncClient_auth\", response=\"4a5ec2fdc1d7dd43dd6fb345944583c5\", "
        "algorithm=\"MD5\"";

const char* kResponseForLowercaseMd5DigestHeader =
        "Digest username=\"zorz_user\", realm=\"Http Server\", "
        "nonce=\"f3164f6a1801ecb0870af2c468a6d7af\", uri=\"/HttpAsyncClient_auth\", "
        "response=\"900afe7b3e6d522346fcfaaa897b6b35\", algorithm=\"md5\", cnonce=\"0a4f113b\", "
        "nc=\"00000001\", qop=\"auth\"";

} // namespace

void AuthHttpServer::processConnection(AbstractStreamSocket* connection)
{
    connection->setRecvTimeout(socketTimeout);
    connection->setSendTimeout(socketTimeout);

    auto [rc, request] = readRequest(connection);
    while (rc > 0)
    {
        m_receivedRequests.push_back(std::move(request));
        sendResponse(connection);
        std::tie(rc, request) = readRequest(connection);
    }

    ASSERT_EQ(0, rc) << "System error: " << SystemError::getLastOSErrorText().toStdString();
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

    ASSERT_EQ(response.size(), connection->send(response.constData(), response.size()));
}


//-------------------------------------------------------------------------------------------------
HttpClientAsyncAuthorization::HttpClientAsyncAuthorization():
    m_httpClient(std::make_unique<http::AsyncClient>())
{
}

HttpClientAsyncAuthorization::~HttpClientAsyncAuthorization()
{
    m_httpClient->pleaseStopSync();
}

void HttpClientAsyncAuthorization::givenHttpServerWithAuthorization(std::vector<AuthHttpServer::AuthHeader> authData)
{
    m_httpServer = std::make_unique<AuthHttpServer>();
    ASSERT_TRUE(m_httpServer->bindAndListen(SocketAddress::anyPrivateAddress));

    for (auto &headerAndResponse: authData)
        m_httpServer->appendAuthHeader(std::move(headerAndResponse));
    m_httpServer->start();
}


void HttpClientAsyncAuthorization::whenClientSendHttpRequestAndIsRequiredToUse(AuthType auth,
    const char *username)
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
            m_httpClient->doGet(url, [&done]() { done.set_value(); });
        });
    done.get_future().wait();
}

void HttpClientAsyncAuthorization::thenClientAuthenticatedBy(const char* exptectedHeaderResponse)
{
    const auto requests = m_httpServer->receivedRequests();

    // No auth exptected.
    if (exptectedHeaderResponse == nullptr) {
        ASSERT_EQ(requests.size(), 1);
        return;
    }

    ASSERT_EQ(requests.size(), 2);
    ASSERT_THAT(requests[1].toStdString(), testing::HasSubstr(exptectedHeaderResponse));
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
    ASSERT_THAT(m_httpServer->receivedRequests().back().toStdString(),
        testing::HasSubstr(usernameSubstring.str()));
}

const AuthHttpServer::AuthHeader basic = {AuthType::authBasic, kDefaultBasicHeader};
const AuthHttpServer::AuthHeader digest = {AuthType::authDigest, kDefaultDigestHeader};
const auto basicResponse = kResponseForDefaultBasicHeader;
const auto digestResponse = kResponseForDefaultDigestHeader;

INSTANTIATE_TEST_CASE_P(HttpClientAsyncAuthorizationInstance, HttpClientAsyncAuthorization,
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
    thenClientAuthenticatedBy(params.expectedAuthResponse);
}

TEST_F(HttpClientAsyncAuthorization, lowercaseAlgorithm)
{
    givenHttpServerWithAuthorization({{AuthType::authDigest, kLowercaseMd5DigestHeader}});
    whenClientSendHttpRequestAndIsRequiredToUse(AuthType::authDigest);
    thenClientGotResponseWithCode(200);
    thenClientAuthenticatedBy(kResponseForLowercaseMd5DigestHeader);
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
