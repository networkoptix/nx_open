// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/server/http_server_builder.h>
#include <nx/network/http/server/proxy/reverse_proxy_handler.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::http::server::proxy::test {

class HttpReverseProxy:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        Settings settings;
        settings.endpoints.push_back(SocketAddress::anyPrivateAddressV4);

        SystemError::ErrorCode err = SystemError::noError;
        std::tie(m_proxyServer, err) = Builder::build(settings, &m_reverseProxyHandler);
        ASSERT_EQ(SystemError::noError, err);

        ASSERT_TRUE(m_proxyServer->listen());
    }

    void givenProxyTargets(std::vector<std::string> targets)
    {
        for (const auto& path: targets)
        {
            m_targets.push_back(std::make_unique<TestHttpServer>());
            auto& server = m_targets.back();
            server->registerStaticProcessor(".*", path, "text/plain");
            ASSERT_TRUE(server->bindAndListen(SocketAddress::anyPrivateAddressV4));

            m_reverseProxyHandler.add(
                Method::get, path,
                url::Builder().setScheme("http").setEndpoint(server->serverAddress()));
        }
    }

    void assertRequestProxiedTo(const std::string& requestPath, const std::string& expectedTarget)
    {
        HttpClient client(ssl::kAcceptAnyCertificate);
        ASSERT_TRUE(client.doGet(url::Builder(m_proxyServer->urls().front()).setPath(requestPath)));
        ASSERT_EQ(StatusCode::ok, client.response()->statusLine.statusCode);
        ASSERT_EQ(expectedTarget, client.fetchEntireMessageBody()->toStdString());
    }

private:
    std::unique_ptr<MultiEndpointServer> m_proxyServer;
    std::vector<std::unique_ptr<TestHttpServer>> m_targets;
    ReverseProxyHandler m_reverseProxyHandler;
};

TEST_F(HttpReverseProxy, path_rules_are_checked_in_reverse_length_order)
{
    givenProxyTargets({"/foo/bar", ".*"});

    assertRequestProxiedTo("/foo/bar", "/foo/bar");
    assertRequestProxiedTo("/foo/bar1", ".*");
}

} // namespace nx::network::http::server::proxy::test
