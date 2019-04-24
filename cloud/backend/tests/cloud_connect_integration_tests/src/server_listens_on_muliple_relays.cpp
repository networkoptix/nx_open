#include "basic_test_fixture.h"

#include <nx/cloud/mediator/relay/relay_cluster_client_factory.h>
#include <nx/cloud/mediator/relay/relay_cluster_client.h>
#include <nx/cloud/mediator/settings.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>

namespace nx::network::cloud::test {

class MultipleRelays;

namespace {

class HardCodedRelayClusterClient:
    public nx::hpm::RelayClusterClient
{
    // Inheriting from actual RelayClusterClient, which reads from settings
    using base_type = nx::hpm::RelayClusterClient;
public:
    HardCodedRelayClusterClient(
        const nx::hpm::conf::Settings& settings,
        MultipleRelays* testFixture);

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        nx::hpm::RelayInstanceSelectCompletionHandler completionHandler) override;

private:
    MultipleRelays * m_testFixture = nullptr;
};

} // namespace

class MultipleRelays:
    public BasicTestFixture,
    public testing::Test
{
    using base_type = BasicTestFixture;

public:
    ~MultipleRelays()
    {
        for (int i = 0; i < m_relayClusterSize; ++i)
            trafficRelay(i).stop();

        if (m_relayClusterClientFuncBak)
        {
            nx::hpm::RelayClusterClientFactory::instance().setCustomFunc(
                std::move(m_relayClusterClientFuncBak));
        }
    }

    void reportTrafficRelayUrlsForServer(const std::vector<QUrl>& trafficRelayUrls)
    {
        m_reportedRelayUrlsForServerPromise.set_value(trafficRelayUrls);
    }

protected:
    virtual void SetUp() override //< Overrides testing::Test, not BasicTestFixture
    {
        setRelayCount(m_relayClusterSize);

        // There are three traffic relays.
        // When client and server connect to the one mediator in the test:
        //  - Relay 2 is always provided to the client.
        //  - Relays 0 and 1 are always provided to the listening server.
        // All three relays should be synchronized, so relay 2 will provide the url of
        // either relay 0 or 1 any clients connected to relay 2.
        auto hardCodeTrafficRelaysFunc =
            [this](const nx::hpm::conf::Settings& settings)
                -> std::unique_ptr<nx::hpm::AbstractRelayClusterClient>
            {
                m_expectedRelayUrlsForServer = {
                    relayUrl(0).toQUrl(),
                    relayUrl(1).toQUrl()
                };

                return std::make_unique<HardCodedRelayClusterClient>(
                    settings,
                    this);
            };

        m_relayClusterClientFuncBak =
            nx::hpm::RelayClusterClientFactory::instance().setCustomFunc(
                hardCodeTrafficRelaysFunc);

        base_type::SetUp(); //< Manually calling BasicTestFixture::SetUp
    }

    void givenMultipleRelays()
    {
        // Done in BasicTestFixture's constructor/SetUp()
    }

    void whenServerConnectsToMediator()
    {
        startServer();
    }

    void whenConnectToMediator()
    {
        assertCloudConnectionCanBeEstablished();
    }

    void thenClientCanConnectToServerViaReportedRelays()
    {
        for (const auto& relayUrl : m_reportedRelayUrlsForServer)
            waitForClientToConnectToRelayDirectly(relayUrl);
    }

    void thenMultipleRelaysAreReportedToServer()
    {
        waitForReportedRelaysUrlsForServer();
        ASSERT_EQ(m_expectedRelayUrlsForServer, m_reportedRelayUrlsForServer);
    }

    void thenServerIsListeningOnMultipleRelays()
    {
        waitForServerAvailableOnReportedRelays();
    }

private:
    void waitForServerAvailableOnReportedRelays()
    {
        std::string systemId = cloudSystemCredentials().systemId.toStdString();
        auto& relay = trafficRelay(m_clientRelayIndex);

        const auto shouldWait =
            [this]()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (const auto& expectedUrl : m_expectedRelayUrlsForServer)
                {
                    QString domainAndPort = expectedUrl.adjusted(QUrl::RemoveScheme).toString();
                    if (domainAndPort.startsWith("//"))
                        domainAndPort = domainAndPort.mid(2);

                    if (m_relayDomains.find(domainAndPort.toStdString()) == m_relayDomains.end())
                        return true;
                }
                return false;
            };

        while (shouldWait())
        {
            relay.moduleInstance()->remoteRelayPeerPool().findRelayByDomain(systemId,
                [this](std::string relayDomain)
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!relayDomain.empty()
                    && m_relayDomains.find(relayDomain) == m_relayDomains.end())
                {
                    m_relayDomains.emplace(relayDomain);
                }
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitForClientToConnectToRelayDirectly(const QUrl& relayUrl)
    {
        using namespace nx::cloud::relay;

        std::string actualRelayUrl;
        auto relayClient = std::make_unique<api::Client>(relayUrl.toString());

        for (;;)
        {
            std::promise<
                std::tuple<api::ResultCode, api::CreateClientSessionResponse>
            > requestCompletion;

            relayClient->startSession(
                "",
                serverSocketCloudAddress(),
                [this, &requestCompletion](
                    api::ResultCode resultCode,
                    api::CreateClientSessionResponse response)
            {
                requestCompletion.set_value(std::make_tuple(resultCode, std::move(response)));
            });

            const auto result = requestCompletion.get_future().get();
            const auto resultCode = std::get<0>(result);
            if (resultCode == api::ResultCode::ok)
            {
                // Verify that there was no redirect
                ASSERT_EQ(relayUrl.toString().toStdString(), std::get<1>(result).actualRelayUrl);
                if (relayUrl.toString().toStdString() == std::get<1>(result).actualRelayUrl)
                    break;
            }

            ASSERT_EQ(api::ResultCode::notFound, resultCode);

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitForReportedRelaysUrlsForServer()
    {
        m_reportedRelayUrlsForServer = m_reportedRelayUrlsForServerPromise.get_future().get();
    }

private:
    int m_relayClusterSize = 3;
    int m_clientRelayIndex = 2;
    nx::utils::MoveOnlyFunc< nx::hpm::RelayClusterClientFactoryFunction>
        m_relayClusterClientFuncBak;

    std::vector<QUrl> m_expectedRelayUrlsForServer;
    std::promise<std::vector<QUrl>> m_reportedRelayUrlsForServerPromise;
    std::vector<QUrl> m_reportedRelayUrlsForServer;

    std::mutex m_mutex;
    std::set<std::string> m_relayDomains;
};

namespace {

HardCodedRelayClusterClient::HardCodedRelayClusterClient(
    const nx::hpm::conf::Settings& settings,
    MultipleRelays* testFixture)
    :
    base_type(settings),
    m_testFixture(testFixture)
{
}

void HardCodedRelayClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& peerId,
    nx::hpm::RelayInstanceSelectCompletionHandler completionHandler)
{
    base_type::selectRelayInstanceForListeningPeer(
        peerId,
        [this, completionHandler = std::move(completionHandler)](
            nx::cloud::relay::api::ResultCode resultCode,
            std::vector<QUrl> relayUrls)
    {
        ASSERT_FALSE(relayUrls.empty());
        relayUrls.pop_back(); //< Dropping relay 2 from the list, it is only for client.

            m_testFixture->reportTrafficRelayUrlsForServer(relayUrls);

        completionHandler(resultCode, std::move(relayUrls));
    });
}

} // namespace

TEST_F(MultipleRelays, mediator_connection_reports_multiple_traffic_relays)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
}

TEST_F(MultipleRelays, client_can_connect_to_server_on_multiple_reported_relays)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
    thenServerIsListeningOnMultipleRelays();

    thenClientCanConnectToServerViaReportedRelays();
}

} //namespace nx::network::cloud::test