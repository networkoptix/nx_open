// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_async_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/http/server/http_server_base_authentication_manager.h>
#include <nx/network/http/server/http_server_plain_text_credentials_provider.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/cryptographic_hash.h>
#include <nx/utils/test_support/utils.h>
#include "nx/network/http/auth_tools.h"
#include "nx/network/http/http_types.h"

namespace nx::network::http::server::test {

namespace {

static const Credentials kDefaultCredentials{
    "user",
    PasswordAuthToken{"PazzWerd"}
};

static const Credentials kInvalidCredentials {
    "user",
    PasswordAuthToken{"pazzwerd"}
};

static constexpr char kDefaultPath[]{"/test"};

} // namespace

class BaseAuthenticationManager:
    public ::testing::Test
{
public:
    BaseAuthenticationManager():
        m_authManager(nullptr, &m_credsProvider)
    {
        m_authManager.setNextHandler(&m_httpServer.httpMessageDispatcher());

        m_credsProvider.addCredentials(kDefaultCredentials);

        m_httpServer.authDispatcher().add(
            std::regex(kDefaultPath),
            &m_authManager);

        NX_GTEST_ASSERT_TRUE(
            m_httpServer.registerStaticProcessor(
                kDefaultPath,
                "hello",
                "text/plain",
                Method::get));

        NX_GTEST_ASSERT_TRUE(m_httpServer.bindAndListen());
    }

    void addCredentials(const Credentials& creds)
    {
        m_credsProvider.addCredentials(creds);
    }

    void addCredentials(std::string_view user, std::string password, AuthTokenType authTokenType)
    {
        std::string actualPassword = authTokenType == AuthTokenType::ha1
            ? calcHa1(user, m_authManager.realm(), password, "MD5")
            : password;

        addCredentials(
            Credentials{
                user,
                AuthToken{std::move(actualPassword), authTokenType}});
    }

    nx::utils::Url httpsUrl(std::string_view path) const
    {
        return url::Builder()
            .setScheme(kSecureUrlSchemeName)
            .setEndpoint(m_httpServer.serverAddress())
            .setPath(path);
    }

    std::optional<Response> makeRequest(
        const Method& method,
        const nx::utils::Url& url,
        std::optional<AuthType> clientAuthType,
        std::optional<Credentials> creds)
    {
        AsyncClient client{ssl::kAcceptAnyCertificate};
        client.setTimeouts(AsyncClient::kInfiniteTimeouts);

        if (clientAuthType)
            client.setAuthType(*clientAuthType);

        if (creds)
            client.setCredentials(*creds);

        std::promise<void> p;
        client.doRequest(method, url, [&p](){ p.set_value(); });
        p.get_future().get();

        std::optional<Response> response;
        if (client.response())
            response.emplace(std::move(*client.response()));

        client.pleaseStopSync();

        return response;
    }

private:
    PlainTextCredentialsProvider m_credsProvider;
    server::BaseAuthenticationManager m_authManager;
    TestHttpServer m_httpServer;
};

TEST_F(BaseAuthenticationManager, https_no_authorization_rejected)
{
    const auto response = makeRequest(
        Method::get,
        httpsUrl(kDefaultPath),
        std::nullopt /*clientAuthType*/,
        std::nullopt /*creds*/);

    ASSERT_EQ(StatusCode::unauthorized, response->statusLine.statusCode);
}

TEST_F(BaseAuthenticationManager, https_basic_authorization_accepted)
{
    const auto response = makeRequest(
        Method::get,
        httpsUrl(kDefaultPath),
        AuthType::authBasic,
        kDefaultCredentials);

    ASSERT_EQ(StatusCode::ok, response->statusLine.statusCode);
}

TEST_F(BaseAuthenticationManager, https_basic_authorization_invalid_credentials_rejected)
{
    const auto reponse = makeRequest(
        Method::get,
        httpsUrl(kDefaultPath),
        AuthType::authBasic,
        kInvalidCredentials);

    ASSERT_EQ(StatusCode::unauthorized, reponse->statusLine.statusCode);
}

TEST_F(BaseAuthenticationManager, https_basic_authorization_accepted_against_ha1_credentials)
{
    Credentials creds{"basicdude", PasswordAuthToken{"PazzWerd"}};

    // This tests exists to test that Basic Authorization with plain text credentials can
    // authenticate successfully against ha1 credentials stored in the
    // BaseAuthenticationManager, so credentials must be encoded into ha1.
    addCredentials(creds.username, creds.authToken.value, AuthTokenType::ha1);

    const auto response = makeRequest(
        Method::get,
        httpsUrl(kDefaultPath),
        AuthType::authBasic,
        creds);

    ASSERT_EQ(StatusCode::ok, response->statusLine.statusCode);
}

TEST_F(BaseAuthenticationManager, https_basic_authorization_invalid_credentials_rejected_against_ha1_credentials)
{
    addCredentials("httpsguy", "PazzWerd", AuthTokenType::ha1);

    const auto response = makeRequest(
        Method::get,
        httpsUrl(kDefaultPath),
        AuthType::authBasic,
        Credentials{"httpsguy", PasswordAuthToken{"pazzwerd"}});

    ASSERT_EQ(StatusCode::unauthorized, response->statusLine.statusCode);
}

} // namespace nx::network::http::server::test
