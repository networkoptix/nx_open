#include <cmath>

#include <gtest/gtest.h>

#include <common/common_module.h>
#include <rest/server/json_rest_result.h>
#include <nx_ec/ec_api.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <test_support/appserver2_process.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/custom_headers.h>
#include <nx/vms/api/data/module_information.h>
#include <transaction/message_bus_adapter.h>
#include <test_support/mediaserver_launcher.h>
#include <api/global_settings.h>
#include <nx/network/url/url_builder.h>

using namespace nx::utils::test;

namespace nx {
namespace vms::server {
namespace proxy {
namespace test {

enum class SslMode
{
    noSsl,
    withSsl,
    withSslAndRedirect, //< Turn on encryption but send HTTP request to cause redirect on 1-st host.
};

struct TestMode
{
    SslMode sslMode;
    bool useReverseConnect = false;
};

class ProxyTest: public ::testing::TestWithParam<TestMode>
{
public:
    ProxyTest()
    {
        NX_VERBOSE(this, lm("Start proxy test"));
        ec2::Appserver2Process::resetInstanceCounter();
    }
    ~ProxyTest()
    {
        NX_VERBOSE(this, lm("End proxy test"));
    }

public:
    std::vector<std::unique_ptr<MediaServerLauncher>> m_peers;

    void startPeers(int peerCount, SslMode sslMode)
    {
        while (m_peers.size() < peerCount)
        {
            auto peer = std::make_unique<MediaServerLauncher>();
            peer->start();
            NX_VERBOSE(this, "Server#%1 (%2) started on %3",
                m_peers.size(), peer->commonModule()->moduleGUID(), peer->endpoint());

            auto settings = peer->serverModule()->globalSettings();
            settings->setTrafficEncriptionForced(sslMode != SslMode::noSsl);
            settings->synchronizeNowSync();

            m_peers.push_back(std::move(peer));
        }
    }

    void connectPeers(SslMode sslMode, bool reverseOrder)
    {
        if (reverseOrder)
        {
            for (int i = 0; i < m_peers.size() - 1; ++i)
                m_peers[i+1]->connectTo(m_peers[i].get(), sslMode != SslMode::noSsl);
        }
        else
        {
            for (int i = 0; i < m_peers.size() - 1; ++i)
                m_peers[i]->connectTo(m_peers[i + 1].get(), sslMode != SslMode::noSsl);
        }
    }

    nx::utils::Url serverUrl(int index, const QString& path, SslMode sslMode)
    {
        const auto endpoint = m_peers[index]->endpoint();
        using namespace nx::network;

        const auto urlScheme = sslMode == SslMode::withSsl
            ? http::kSecureUrlSchemeName : http::kUrlSchemeName;
        return nx::network::url::Builder().setEndpoint(endpoint).setScheme(urlScheme).setPath(path);
    }
};

TEST_P(ProxyTest, proxyToAnotherThenToThemself)
{
    const TestMode testMode = GetParam();
    startPeers(3, testMode.sslMode);
    connectPeers(testMode.sslMode, testMode.useReverseConnect);

    auto messageBus = m_peers[0]->serverModule()->ec2Connection()->messageBus();
    auto server2Id = m_peers[m_peers.size() - 1]->commonModule()->moduleGUID();
    int distance = std::numeric_limits<int>::max();
    while (distance > m_peers.size())
    {
        distance = messageBus->distanceToPeer(server2Id);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto client = std::make_unique<nx::network::http::HttpClient>();

    ASSERT_TRUE(client->doGet(serverUrl(0, "/api/moduleInformation", testMode.sslMode)));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);

    for (int i = m_peers.size() - 1; i >= 0; --i)
    {
        const auto guid = m_peers[i]->commonModule()->moduleGUID();
        client->removeAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME);
        // Media server should proxy to itself if header is missing.
        if (i > 0)
            client->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, guid.toByteArray());

        const auto requestUrl =
            serverUrl(0, "/api/moduleInformation", testMode.sslMode);
        NX_VERBOSE(this, "Sending request %1 to server#%2", requestUrl, i);
        ASSERT_TRUE(client->doGet(requestUrl));
        ASSERT_TRUE(client->response());
        ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);

        bool success = false;
        auto response = client->fetchEntireMessageBody();
        ASSERT_TRUE(response);
        QnJsonRestResult json =
            QJson::deserialized<QnJsonRestResult>(*response, QnJsonRestResult(), &success);
        auto moduleInformation = json.deserialized<nx::vms::api::ModuleInformation>();

        ASSERT_EQ(guid, moduleInformation.id);
    }
    int totalRequests = client->totalRequestsSent();
    int viaCurrentConnection = client->totalRequestsSentViaCurrentConnection();

    if (testMode.sslMode != SslMode::withSslAndRedirect)
        ASSERT_EQ(totalRequests, viaCurrentConnection);
}

INSTANTIATE_TEST_CASE_P(TestMode, ProxyTest,
    ::testing::Values(
        TestMode{SslMode::noSsl},
        TestMode{SslMode::withSsl},
        TestMode{SslMode::withSslAndRedirect},
        TestMode{SslMode::noSsl, /*reverseConnect*/ true},
        TestMode{SslMode::withSsl, /*reverseConnect*/ true},
        TestMode{SslMode::withSslAndRedirect, /*reverseConnect*/ true}));

} // namespace test
} // namespace proxy
} // namespace vms::server
} // namespace nx
