#include "basic_test_fixture.h"

#include <nx/cloud/mediator/settings.h>
#include <nx/cloud/mediator/geo_ip/resolver_factory.h>
#include <nx/cloud/mediator/relay/relay_cluster_client_factory.h>
#include <nx/cloud/mediator/relay/online_relays_cluster_client.h>
#include <nx/geo_ip/test_support/memory_resolver.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/cloud_connect_controller.h>

namespace nx::network::cloud::test {

class MultipleRelays;

namespace {

nx::network::SocketAddress getEndpoint(const nx::utils::Url& url)
{
    return nx::network::SocketAddress(url.host(), url.port());
}

class NotifyingRelayClusterClient:
    public nx::hpm::OnlineRelaysClusterClient
{
    // Inheriting from actual RelayClusterClient, which reads from settings
    using base_type = nx::hpm::OnlineRelaysClusterClient;
public:
    NotifyingRelayClusterClient(
        const nx::hpm::conf::Settings& settings,
        MultipleRelays* testFixture);

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        const nx::network::SocketAddress& serverEndpoint,
        nx::hpm::RelayInstanceSelectCompletionHandler completionHandler) override;

    void mockupServerRegion(const nx::network::SocketAddress& serverEndpoint);

private:
    MultipleRelays * m_testFixture = nullptr;
    bool m_serverRegionMockedUp = false;
};

} // namespace

class MultipleRelays:
    public BasicTestFixture,
    public testing::Test
{
    using base_type = BasicTestFixture;

    friend class NotifyingRelayClusterClient;

public:
    ~MultipleRelays()
    {
        discoveryServer().unsubscribeFromNodeDiscovered(m_nodeDiscoveredSubscriptionId);

        for (int i = 0; i < m_relayClusterSize; ++i)
            trafficRelay(i).stop();

        if (m_geoIpFactoryFuncBak)
        {
            nx::hpm::geo_ip::ResolverFactory::instance().setCustomFunc(
                std::move(m_geoIpFactoryFuncBak));
        }

        if (m_relayClusterClientFuncBak)
        {
            nx::hpm::RelayClusterClientFactory::instance().setCustomFunc(
                std::move(m_relayClusterClientFuncBak));
        }
    }

    void reportTrafficRelayUrlsForServer(const std::vector<nx::utils::Url>& trafficRelayUrls)
    {
        m_reportedRelayUrlsForServerPromise.set_value(trafficRelayUrls);
    }

protected:
    virtual void SetUp() override //< Overrides testing::Test, not BasicTestFixture
    {
        // There are three traffic relays, one mediator, and one mediaserver.
        //  - Relays 0 and 1 live in NorthAmerica
        //  - The mediaserver also lives in North America
        //  - Relay 2 lives in Australia
        //  - The client connects to Australia relay.
        // 1. When the server connects to mediator, it should receive exactly two relay urls,
        // the two from North America, and NOT the one from Australia. Any other result fails.
        // 2. When the client connects to the mediator, it should receive a relay url from North America,
        // and NOT the one from Australia. Any other result fails.
        // 3. The client should be able to connect to all relay urls that the mediaserver is
        // listening on (the two form North America),
        // 4. The relay from Australia should synchronize information about listening servers
        // with the two relays from North America. This means that all three relays have two
        // listening peer entries in their AbtractRemoteRelayPeerPool:
        //  - one entry for each relay <--> mediaserver connection.
        // The test continues until all these condiions have passed.

        using namespace nx::hpm;

        m_relayClusterSize = 3;
        m_clientRelayIndex = 2; //< Client connects to Australia relay.

        setRelayCount(m_relayClusterSize);

        m_nodeDiscoveredSubscriptionId = discoveryServer().subscribeToNodeDiscovered(
            [this](std::string clusterId, nx::cloud::discovery::NodeInfo nodeInfo)
            {
                if (clusterId == relayClusterId())
                {
                    discoveryServer().mockupClientPublicIpAddress(
                        nodeInfo.nodeId,
                        getEndpoint(nodeInfo.urls.front()).toStdString());
                }
            });
        ASSERT_NE(m_nodeDiscoveredSubscriptionId, nx::utils::kInvalidSubscriptionId);

        m_geoIpFactoryFuncBak = nx::hpm::geo_ip::ResolverFactory::instance().setCustomFunc(
            [this](const conf::Settings& /*settings*/)
            {
                auto geoIpResolver = std::make_unique<nx::geo_ip::test::MemoryResolver>();
                m_geoIpResolver = geoIpResolver.get();
                return geoIpResolver;
            });

        m_relayClusterClientFuncBak = RelayClusterClientFactory::instance().setCustomFunc(
            [this](const conf::Settings& settings)
            {
                auto relayClusterClient = std::make_unique<NotifyingRelayClusterClient>(
                    settings,
                    this);
                m_relayClusterClient = relayClusterClient.get();
                mockupRelayRegions();
                return relayClusterClient;
            });

        base_type::SetUp(); //< Manually calling BasicTestFixture::SetUp
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

        ASSERT_EQ(m_expectedRelayUrlsForServer.size(), m_reportedRelayUrlsForServer.size());
        for (auto& expectedUrl : m_expectedRelayUrlsForServer)
        {
            auto it = std::find(
                m_reportedRelayUrlsForServer.begin(),
                m_reportedRelayUrlsForServer.end(),
                expectedUrl);

            ASSERT_TRUE(it != m_reportedRelayUrlsForServer.end());
        }
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
                    auto endpoint = getEndpoint(expectedUrl);

                    if (m_reportedRelayDomains.find(endpoint.toStdString())
                        == m_reportedRelayDomains.end())
                    {
                        return true;
                    }
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
                        && m_reportedRelayDomains.find(relayDomain) == m_reportedRelayDomains.end())
                    {
                        m_reportedRelayDomains.emplace(relayDomain);
                    }
                });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitForClientToConnectToRelayDirectly(const nx::utils::Url& relayUrl)
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
                ASSERT_EQ(relayUrl.toStdString(), std::get<1>(result).actualRelayUrl);
                if (relayUrl.toStdString() == std::get<1>(result).actualRelayUrl)
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

    void mockupRelayRegions()
    {
        ASSERT_TRUE(m_geoIpResolver != nullptr);

        // NOTE: server region can't be mocked up until it is started because its SocketAddress is
        // needed. It is done inside
        // NotifyingRelayClusterClient::selectRelayInstanceForListeningPeer()

        {
            using namespace nx::geo_ip;
            m_geoIpResolver->add(getEndpoint(relayUrl(0)), {Continent::northAmerica});
            m_geoIpResolver->add(getEndpoint(relayUrl(1)), {Continent::northAmerica});
            m_geoIpResolver->add(getEndpoint(relayUrl(2)), {Continent::australia});
        }

        m_expectedRelayUrlsForServer = { relayUrl(0), relayUrl(1) };
    }

private:
    int m_relayClusterSize;
    int m_clientRelayIndex;
    nx::utils::SubscriptionId m_nodeDiscoveredSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    nx::utils::MoveOnlyFunc<nx::hpm::geo_ip::ResolverFactoryFunction>
        m_geoIpFactoryFuncBak;

    nx::utils::MoveOnlyFunc<nx::hpm::RelayClusterClientFactoryFunction>
        m_relayClusterClientFuncBak;

    std::vector<nx::utils::Url> m_expectedRelayUrlsForServer;
    std::promise<std::vector<nx::utils::Url>> m_reportedRelayUrlsForServerPromise;
    std::vector<nx::utils::Url> m_reportedRelayUrlsForServer;

    nx::geo_ip::test::MemoryResolver* m_geoIpResolver = nullptr;
    NotifyingRelayClusterClient* m_relayClusterClient = nullptr;

    std::mutex m_mutex;
    std::set<std::string> m_reportedRelayDomains;
};

namespace {

NotifyingRelayClusterClient::NotifyingRelayClusterClient(
    const nx::hpm::conf::Settings& settings,
    MultipleRelays* testFixture)
    :
    base_type(settings),
    m_testFixture(testFixture)
{
}

void NotifyingRelayClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& peerId,
    const nx::network::SocketAddress& serverEndpoint,
    nx::hpm::RelayInstanceSelectCompletionHandler completionHandler)
{
    // NOTE: we need to wait until the server is started to mockup its region, its address
    // isn't determined until it is started.
    mockupServerRegion(serverEndpoint);

    base_type::selectRelayInstanceForListeningPeer(
        peerId,
        serverEndpoint,
        [this, completionHandler = std::move(completionHandler)](
            nx::cloud::relay::api::ResultCode resultCode,
            std::vector<nx::utils::Url> relayUrls)
        {
            m_testFixture->reportTrafficRelayUrlsForServer(relayUrls);
            completionHandler(resultCode, std::move(relayUrls));
        });
}

void NotifyingRelayClusterClient::mockupServerRegion(
    const nx::network::SocketAddress& serverEndpoint)
{
    if (m_serverRegionMockedUp)
        return;

    m_testFixture->m_geoIpResolver->add(
        serverEndpoint,
        {nx::geo_ip::Continent::northAmerica});

    m_serverRegionMockedUp = true;
}

} // namespace

TEST_F(MultipleRelays, mediator_reports_multiple_traffic_relays)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
}

TEST_F(MultipleRelays, client_connects_to_server_on_multiple_reported_relays)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
    thenServerIsListeningOnMultipleRelays();

    thenClientCanConnectToServerViaReportedRelays();
}

} //namespace nx::network::cloud::test