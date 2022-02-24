// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/server/authentication_dispatcher.h>
#include <nx/network/http/server/rest/http_server_rest_message_dispatcher.h>
#include <nx/network/ssl/certificate.h>
#include <nx/network/ssl/context.h>
#include <nx/network/ssl/ssl_stream_socket.h>
#include <nx/network/system_socket.h>
#include <nx/network/url/url_parse_helper.h>

#include <nx/utils/test_support/test_with_temporary_directory.h>

namespace nx::network::http::server::test {

class HttpServerBuilder:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    HttpServerBuilder()
    {
        m_settings.endpoints.push_back("127.0.0.1:0");
        m_settings.ssl.endpoints.push_back("127.0.0.1:0");
    }

    void buildServer()
    {
        SystemError::ErrorCode err = SystemError::noError;
        std::tie(m_server, err) = Builder::build(settings(), nullptr, nullptr);
        ASSERT_EQ(SystemError::noError, err) << SystemError::toString(err);
        ASSERT_NE(nullptr, m_server);
    }

    MultiEndpointServer& server() { return *m_server; }

protected:
    Settings& settings() { return m_settings; }

    void enableSslServerCertificate()
    {
        m_issuer = ssl::X509Name{"nx_network/HttpServerBuilder test", "US", "Nx"};

        settings().ssl.certificatePath = testDataDir().toStdString() + "/test.cert";
        const auto certData = makeCertificateAndKey(m_issuer, "localhost");
        std::ofstream f(
            settings().ssl.certificatePath,
            std::ios_base::binary | std::ios_base::trunc);
        ASSERT_TRUE(f.is_open());
        f << certData;
    }

    void whenEnableRedirectHttpToHttps()
    {
        settings().redirectHttpToHttps = true;
    }

    void whenMakeRequestOnHttp()
    {
        nx::utils::Url httpUrl = *std::find_if(server().urls().begin(), server().urls().end(),
        [] (const auto& url)
        {
            return url.scheme() == nx::network::http::kUrlSchemeName;
        });
        ASSERT_TRUE(m_client.doGet(httpUrl));
    }

    void thenContentLocationUrlIsHttps()
    {
        ASSERT_EQ(m_client.contentLocationUrl().scheme(), nx::network::http::kSecureUrlSchemeName);
    }

    void assertServerUsesTheProvidedCertificate()
    {
        ssl::ClientStreamSocket socket(
            ssl::Context::instance(),
            std::make_unique<TCPSocket>(AF_INET),
            ssl::kAcceptAnyCertificateCallback);

        socket.setSyncVerifyCertificateChainCallback(
            [this](auto&&... args)
            {
                return verifyServerCertificate(std::forward<decltype(args)>(args)...);
            });

        nx::utils::Url httpsUrl = *std::find_if(server().urls().begin(), server().urls().end(),
        [] (const auto& url)
        {
            return url.scheme() == nx::network::http::kSecureUrlSchemeName;
        });

        ASSERT_TRUE(socket.connect(url::getEndpoint(httpsUrl), kNoTimeout))
            << SystemError::getLastOSErrorText();

        ASSERT_EQ(m_issuer, m_lastVerifiedCertificate.issuer());
    }

private:
    bool verifyServerCertificate(
        nx::network::ssl::CertificateChainView chain,
        nx::network::ssl::StreamSocket* /*socket*/)
    {
        m_lastVerifiedCertificate = nx::network::ssl::Certificate(chain.front());
        return true;
    }

private:
    Settings m_settings;
    std::unique_ptr<MultiEndpointServer> m_server;
    HttpClient m_client{ssl::kAcceptAnyCertificate};
    ssl::X509Name m_issuer;
    ssl::Certificate m_lastVerifiedCertificate;
};

TEST_F(HttpServerBuilder, preferred_url_has_listening_endpoint)
{
    buildServer();

    ASSERT_EQ(
        nx::utils::Url(nx::format("https://127.0.0.1:%1").args(server().endpoints().back().port)),
        server().preferredUrl());
}

TEST_F(HttpServerBuilder, preferred_url_has_given_server_name)
{
    settings().serverName = "example.com";

    buildServer();

    ASSERT_EQ(
        nx::utils::Url(nx::format("https://example.com:%1").args(server().endpoints().back().port)),
        server().preferredUrl());
}

TEST_F(HttpServerBuilder, preferred_url_has_given_server_name_and_port)
{
    settings().serverName = "example.com:443";

    buildServer();

    ASSERT_EQ(nx::utils::Url("https://example.com:443"), server().preferredUrl());
}

TEST_F(HttpServerBuilder, redirect_http_to_https)
{
    whenEnableRedirectHttpToHttps();

    buildServer();
    ASSERT_TRUE(server().listen());

    whenMakeRequestOnHttp();

    thenContentLocationUrlIsHttps();
}

TEST_F(HttpServerBuilder, ssl_certificate_is_loaded)
{
    enableSslServerCertificate();

    buildServer();
    ASSERT_TRUE(server().listen());

    assertServerUsesTheProvidedCertificate();
}

} // namespace nx::network::http::server::test
