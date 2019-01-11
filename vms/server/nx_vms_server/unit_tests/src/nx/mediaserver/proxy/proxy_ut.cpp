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

using namespace nx::utils::test;

namespace nx {
namespace vms::server {
namespace proxy {
namespace test {

enum class SslMode
{
    noSsl,
    withSsl,
    withSslAndRedirect //< Turn on encryption but send HTTP request to cause redirect on 1-st host.
};

class ProxyTest: public ::testing::TestWithParam<SslMode>
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

            auto settings = peer->serverModule()->globalSettings();
            settings->setTrafficEncriptionForced(sslMode != SslMode::noSsl);
            settings->synchronizeNowSync();

            m_peers.push_back(std::move(peer));
        }
    }

    void connectPeers(SslMode sslMode)
    {
        for (int i = 0; i < m_peers.size() - 1; ++i)
        {
            // TODO: Create function in MediaServerLauncher.
            const auto& from = m_peers[i];
            const auto& to = m_peers[i + 1];

            auto urlScheme = sslMode != SslMode::noSsl
                ? nx::network::http::kSecureUrlSchemeName : nx::network::http::kUrlSchemeName;
            const auto peerId = to->commonModule()->moduleGUID();
            const auto url = nx::utils::url::parseUrlFields(
                to->endpoint().toString(), urlScheme);

            from->serverModule()->ec2Connection()->messageBus()->addOutgoingConnectionToPeer(
                peerId, nx::vms::api::PeerType::server, url);
        }
    }

    nx::utils::Url serverUrl(int index, const QString& path, SslMode sslMode)
    {
        const auto endpoint = m_peers[index]->endpoint();
        using namespace nx::network;

        auto urlScheme = sslMode == SslMode::withSsl
            ? http::kSecureUrlSchemeName : http::kUrlSchemeName;
        nx::utils::Url url = nx::utils::Url(urlScheme + lit("://") + endpoint.toString());
        url.setPath(path);
        return url;
    }
};

TEST_P(ProxyTest, proxyToAnotherThenToThemself)
{
    SslMode sslMode = GetParam();
    startPeers(3, sslMode);
    connectPeers(sslMode);

    auto messageBus = m_peers[0]->serverModule()->ec2Connection()->messageBus();
    auto server2Id = m_peers[m_peers.size() - 1]->commonModule()->moduleGUID();
    int distance = std::numeric_limits<int>::max();
    while (distance > m_peers.size())
    {
        distance = messageBus->distanceToPeer(server2Id);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    auto client = std::make_unique<nx::network::http::HttpClient>();

    ASSERT_TRUE(client->doGet(serverUrl(0, "/api/moduleInformation", sslMode)));
    ASSERT_TRUE(client->response());
    ASSERT_EQ(nx::network::http::StatusCode::ok, client->response()->statusLine.statusCode);

    for (int i = m_peers.size() - 1; i >= 0; --i)
    {
        const auto guid = m_peers[i]->commonModule()->moduleGUID();
        client->removeAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME);
        // Media server should proxy to itself if header is missing.
        if (i > 0)
            client->addAdditionalHeader(Qn::SERVER_GUID_HEADER_NAME, guid.toByteArray());
        ASSERT_TRUE(client->doGet(serverUrl(0, "/api/moduleInformation", sslMode)));
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

    if (sslMode != SslMode::withSslAndRedirect)
        ASSERT_EQ(totalRequests, viaCurrentConnection);
}

INSTANTIATE_TEST_CASE_P(SslMode, ProxyTest,
    ::testing::Values(SslMode::noSsl, SslMode::withSsl, SslMode::withSslAndRedirect));

} // namespace test
} // namespace proxy
} // namespace vms::server
} // namespace nx
