// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/network/http/http_client.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/maintenance/get_version.h>
#include <nx/network/maintenance/request_path.h>
#include <nx/network/maintenance/server.h>
#include <nx/network/url/url_builder.h>

namespace nx::network::maintenance::test {

struct DebugCounters
{
    int tcpSocketCount = 0;
    int stunServerConnectionCount = 0;
    int httpServerConnectionCount = 0;
};

#define DebugCounters_Fields (tcpSocketCount)(stunServerConnectionCount)(httpServerConnectionCount)

NX_REFLECTION_INSTRUMENT(DebugCounters, DebugCounters_Fields)

//-------------------------------------------------------------------------------------------------

class MaintenanceServer:
    public ::testing::Test
{
protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(m_httpServer.bindAndListen());

        m_maintenanceServer.registerRequestHandlers(
            &m_httpServer.httpMessageDispatcher());

        m_httpClient.setResponseReadTimeout(kNoTimeout);
        m_httpClient.setMessageBodyReadTimeout(kNoTimeout);
    }

    void whenRequestMallocInfo()
    {
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kMallocInfo)));
    }

    void whenFetchDebugCounters()
    {
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kDebugCounters)));
    }

    void whenRequestVersion()
    {
        ASSERT_TRUE(m_httpClient.doGet(requestUrl(kVersion)));
    }

    void thenRequestSucceeded()
    {
        ASSERT_NE(nullptr, m_httpClient.response());
        ASSERT_EQ(http::StatusCode::ok, m_httpClient.response()->statusLine.statusCode);
    }

    void andMallocInfoIsProvided()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);
        ASSERT_FALSE(msgBody->empty());

        // TODO: #akolesnikov Add stronger validation.
    }

    void andDebugCountersReceived()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);

        const auto [debugCounters, result] = nx::reflect::json::deserialize<DebugCounters>(
            (nx::ConstBufferRefType) *msgBody);
        ASSERT_EQ(1U, debugCounters.httpServerConnectionCount);
        ASSERT_EQ(2U, debugCounters.tcpSocketCount);
        ASSERT_EQ(0U, debugCounters.stunServerConnectionCount);
    }

    void andVersionIsProvided()
    {
        const auto msgBody = m_httpClient.fetchEntireMessageBody();
        ASSERT_TRUE(msgBody);

        const auto [version, result] = nx::reflect::json::deserialize<Version>(*msgBody);

        ASSERT_EQ(getVersion().version, version.version);
        ASSERT_EQ(getVersion().revision, version.revision);
    }

private:
    http::TestHttpServer m_httpServer;
    maintenance::Server m_maintenanceServer{""};
    http::HttpClient m_httpClient{ssl::kAcceptAnyCertificate};

    nx::utils::Url requestUrl(const std::string& requestName) const
    {
        return url::Builder().setEndpoint(m_httpServer.serverAddress())
            .setScheme(http::kUrlSchemeName).setPath(kMaintenance)
            .appendPath(requestName).toUrl();
    }
};

TEST_F(MaintenanceServer, malloc_info)
{
    whenRequestMallocInfo();

    thenRequestSucceeded();
    andMallocInfoIsProvided();
}

TEST_F(MaintenanceServer, debug_counters)
{
    whenFetchDebugCounters();

    thenRequestSucceeded();
    andDebugCountersReceived();
}

TEST_F(MaintenanceServer, version)
{
    whenRequestVersion();

    thenRequestSucceeded();
    andVersionIsProvided();
}

} // namespace nx::network::maintenance::test
