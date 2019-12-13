#include "basic_test_fixture.h"

#include <nx/cloud/mediator/settings.h>
#include <nx/cloud/mediator/geo_ip/resolver_factory.h>
#include <nx/cloud/mediator/relay/relay_cluster_client_factory.h>
#include <nx/cloud/mediator/relay/online_relays_cluster_client.h>
#include <nx/geo_ip/test_support/memory_resolver.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/cloud/mediator_connector.h>
#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/http/buffer_source.h>
#include <nx/network/url/url_parse_helper.h>

namespace nx::network::cloud::test {

class MultipleRelays;

namespace {

class NotifyingRelayClusterClient:
    public nx::hpm::OnlineRelaysClusterClient
{
    // Inheriting from actual RelayClusterClient, which reads from settings
    using base_type = nx::hpm::OnlineRelaysClusterClient;
public:
    NotifyingRelayClusterClient(
        const nx::hpm::conf::Settings& settings,
        nx::geo_ip::AbstractResolver* resolver,
        MultipleRelays* testFixture);

    virtual void selectRelayInstanceForListeningPeer(
        const std::string& peerId,
        const nx::network::HostAddress& serverHost,
        nx::hpm::RelayInstanceSelectCompletionHandler completionHandler) override;

    virtual void findRelayInstanceForClient(
        const std::string& peerId,
        const nx::network::HostAddress& clientHost,
        nx::hpm::RelayInstanceSearchCompletionHandler completionHandler) override;

private:
    MultipleRelays* m_testFixture = nullptr;
};

} // namespace

class MultipleRelays:
    public BasicTestFixture<::testing::Test>
{
    using base_type = BasicTestFixture<::testing::Test>;

public:
    void addIpAndRegion(const std::string& ipAddress, const nx::geo_ip::Location& location)
    {
        m_geoIpResolver->add(ipAddress, location);
    }

    void reportTrafficRelayUrlsForServer(const std::vector<nx::utils::Url>& trafficRelayUrls)
    {
        m_reportedRelayUrlsForServerPromise.set_value(trafficRelayUrls);
    }

    void reportTrafficRelayUrlForClient(const nx::utils::Url& trafficRelayUrl)
    {
        m_reportedRelayUrlForClientPromise.set_value(trafficRelayUrl);
    }

    void reportClientHostAddress(const network::HostAddress& hostAddress)
    {
        m_reportedClientHostAddressPromise.set_value(hostAddress);
    }

    const std::string& hardCodedClientIp()
    {
        return m_hardCodedClientIp;
    }

protected:
    virtual void SetUp() override //< Overrides testing::Test, not BasicTestFixture
    {
        // There are three traffic relays, one mediator, and one mediaserver.
        //  - Relays 0 and 1 live in NorthAmerica
        //  - The mediaserver also lives in North America
        //  - Relay 2 lives in Australia
        //  - The client lives in Australia.
        // 1. When the server connects to mediator, it should receive exactly two relay urls,
        // the two from North America, and NOT the one from Australia. Any other result fails.
        // 2. When the client connects to the mediator, it should receive a relay url from Australia,
        // and NOT the one from North America. Any other result fails.
        // 3. The client should be able to connect to all relay urls that the mediaserver is
        // listening on (the two form North America),
        // 4. The relay from Australia should synchronize information about listening servers
        // with the two relays from North America. This means that all three relays have two
        // listening peer entries in their AbtractRemoteRelayPeerPool:
        //  - one entry for each relay <--> mediaserver connection.
        // The test continues until all these condiions have passed.

        using namespace nx::hpm;

        setRelayCount(3);
        m_clientRelayIndex = 2; //< Client connects to Australia relay.
        m_hardCodedClientIp = "45.76.116.155"; // Some IP in Australia.

        discoveryServer().subscribeToNodeDiscovered(
            [this](const auto& clusterId, const auto& nodeInfo)
            {
                if (clusterId == relayClusterId())
                {
                    std::lock_guard<std::mutex> lock(m_mutex);

                    NX_DEBUG(this, "Discovery server found a traffic relay: %1", nodeInfo);

                    auto publicIpAddress = lm("10.10.10.%1").arg(++m_relayIpHostPart).toStdString();

                    discoveryServer().mockupClientPublicIpAddress(
                        nodeInfo.nodeId,
                        publicIpAddress);

                    // note: continent can't be mocked up yet because not all relays have been
                    // started. There is no way to safely compare this relay's url to
                    // relayUrl(1) or (2)

                    nx::utils::Url url(nodeInfo.urls.front());
                    url.setPath({});

                    m_relayIpMockup.emplace(std::move(url), publicIpAddress);
                }
            },
            &m_nodeDiscoveredSubscriptionId);
        ASSERT_NE(nx::utils::kInvalidSubscriptionId, m_nodeDiscoveredSubscriptionId);

        m_geoIpFactoryFuncBak = nx::hpm::geo_ip::ResolverFactory::instance().setCustomFunc(
            [this](const conf::Settings& /*settings*/)
            {
                auto geoIpResolver = std::make_unique<nx::geo_ip::test::MemoryResolver>();
                m_geoIpResolver = geoIpResolver.get();
                mockupRelayRegions(m_geoIpResolver);
                return geoIpResolver;
            });
        m_relayClusterClientFuncBak = RelayClusterClientFactory::instance().setCustomFunc(
            [this](const conf::Settings& settings, nx::geo_ip::AbstractResolver* resolver)
            {
                auto relayClusterClient = std::make_unique<NotifyingRelayClusterClient>(
                    settings,
                    resolver,
                    this);
                m_relayClusterClient = relayClusterClient.get();
                return relayClusterClient;
            });

        base_type::SetUp(); //< Manually calling BasicTestFixture::SetUp
    }

    virtual void TearDown() override
    {
        discoveryServer().unsubscribeFromNodeDiscovered(m_nodeDiscoveredSubscriptionId);

        for (int i = 0; i < relayClusterSize(); ++i)
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

        base_type::TearDown();
    }

    void whenServerConnectsToMediator()
    {
        startServer();
    }

    void whenConnectToMediator()
    {
        assertCloudConnectionCanBeEstablished();
    }

    network::http::HttpHeader xForwardedForHeader() const
    {
        return network::http::HttpHeader(
            network::http::header::XForwardedFor::NAME,
            (m_hardCodedClientIp + ", 1.2.3.5, 1.2.3.6").c_str());
    }

    network::http::HttpHeader forwardedHeader()
    {
        return network::http::HttpHeader(
            network::http::header::Forwarded::NAME,
            ("for=" + m_hardCodedClientIp + "; for=1.2.3.5; for=1.2.3.6").c_str());
    }

    void whenClientBehindProxyConnectsToMediator(const network::http::HttpHeader& header)
    {
        //Need to start a mediaserver to avoid 404 result because systemId does not exist
        startServer(0);

        prepareMediatorClientBehindProxy(header);

        auto url = nx::network::url::Builder(mediator(0).httpUrl()).appendPath(
            nx::network::http::rest::substituteParameters(
            nx::hpm::api::kServerSessionsPath, {cloudSystemCredentials().systemId}));

        m_mediatorClientBehindProxy.doPost(
            url,
            [this]() { processClientBehindProxyConnectResponse(); });
    }

    void thenClientReceivesRelayUrl()
    {
        waitForReportedRelayUrlForClient();
    }

    void andClientAndReportedRelayAreInTheSameRegion()
    {
        ASSERT_EQ(m_expectedRelayUrlForClient, m_reportedRelayUrlForClient);
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

    void thenMediatorHasCorrectClientHostAddress()
    {
        const auto clientHostAddress =
            m_reportedClientHostAddressPromise.get_future().get().toStdString();
        ASSERT_EQ(m_hardCodedClientIp, clientHostAddress);
    }

    void andConnectRequestBehindProxySucceeds()
    {
        // Verifying that the url reported to client is the expected one
        thenClientReceivesRelayUrl();
        andClientAndReportedRelayAreInTheSameRegion();

        const auto [resultCode, connectResponse] = m_clientBehindProxyConnectResponse.pop();
        ASSERT_EQ(nx::hpm::api::ResultCode::ok, resultCode);
        ASSERT_EQ(cloudSystemCredentials().hostName(), connectResponse.destinationHostFullName);
    }

private:
    void prepareMediatorClientBehindProxy(const network::http::HttpHeader& header)
    {
        nx::hpm::api::ConnectRequest connectRequest;
        connectRequest.destinationHostName = cloudSystemCredentials().systemId;
        connectRequest.originatingPeerId = nx::utils::generateRandomName(7);
        connectRequest.connectSessionId = nx::utils::generateRandomName(7);
        connectRequest.connectionMethods = nx::hpm::api::ConnectionMethod::all;

        m_mediatorClientBehindProxy.addAdditionalHeader(header.first, header.second);

        m_mediatorClientBehindProxy.setRequestBody(
            std::make_unique<nx::network::http::BufferSource>(
                nx::network::http::header::ContentType::kJson,
                QJson::serialized(connectRequest)));
    }

    void processClientBehindProxyConnectResponse()
    {
        bool ok = false;
        const auto connectResponse = QJson::deserialized(
            m_mediatorClientBehindProxy.fetchMessageBodyBuffer(),
            nx::hpm::api::ConnectResponse(),
            &ok);

        const auto resultCode = nx::hpm::api::Client::getResultCode(
            m_mediatorClientBehindProxy.lastSysErrorCode(),
            m_mediatorClientBehindProxy.response(),
            connectResponse);

        m_clientBehindProxyConnectResponse.push({resultCode, std::move(connectResponse)});
    }

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
                    auto endpoint = nx::network::url::getEndpoint(expectedUrl);

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
            relay.moduleInstance()->remoteRelayPeerPool().findRelayByDomain(
                systemId,
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
        auto relayClient = std::make_unique<api::Client>(relayUrl);

        for (;;)
        {
            std::promise<
                std::tuple<api::ResultCode, api::CreateClientSessionResponse>
            > requestCompletion;

            relayClient->startSession(
                "",
                serverSocketCloudAddress(),
                [&requestCompletion](
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

    void waitForReportedRelayUrlForClient()
    {
        m_reportedRelayUrlForClient = m_reportedRelayUrlForClientPromise.get_future().get();
    }

    void mockupRelayRegions(nx::geo_ip::test::MemoryResolver* geoIpResolver)
    {
        const auto needToWaitForRelayIpMockup =
            [this]()
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                return (int)m_relayIpMockup.size() < relayClusterSize();
            };

        ASSERT_NE(geoIpResolver, nullptr);

        while (needToWaitForRelayIpMockup())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

        using namespace nx::geo_ip;
        geoIpResolver->add(
            m_relayIpMockup.at(relayUrl(0)),
            Continent::northAmerica);

        geoIpResolver->add(
            m_relayIpMockup.at(relayUrl(1)),
            Continent::northAmerica);

        geoIpResolver->add(
            m_relayIpMockup.at(relayUrl(2)),
            Continent::australia);

        for (int i = 0; i < relayClusterSize(); ++i)
        {
            auto url = relayUrl(i);
            auto publicIp = m_relayIpMockup.at(url);
            NX_DEBUG(this, "relayUrl(%1): %2 mocked up to public ip: %3, location: %4",
                i, url, publicIp, m_geoIpResolver->resolve(publicIp));
        }

        m_expectedRelayUrlForClient = relayUrl(2);
        m_expectedRelayUrlsForServer = { relayUrl(0), relayUrl(1) };
    }

private:
    int m_clientRelayIndex;
    std::string m_hardCodedClientIp;
    nx::utils::SubscriptionId m_nodeDiscoveredSubscriptionId =
        nx::utils::kInvalidSubscriptionId;

    nx::utils::MoveOnlyFunc<nx::hpm::geo_ip::ResolverFactoryFunction>
        m_geoIpFactoryFuncBak;

    nx::utils::MoveOnlyFunc<nx::hpm::RelayClusterClientFactoryFunction>
        m_relayClusterClientFuncBak;

    std::vector<nx::utils::Url> m_expectedRelayUrlsForServer;
    std::promise<std::vector<nx::utils::Url>> m_reportedRelayUrlsForServerPromise;
    std::vector<nx::utils::Url> m_reportedRelayUrlsForServer;

    nx::utils::Url m_expectedRelayUrlForClient;
    std::promise<nx::utils::Url> m_reportedRelayUrlForClientPromise;
    nx::utils::Url m_reportedRelayUrlForClient;

    nx::geo_ip::test::MemoryResolver* m_geoIpResolver = nullptr;

    std::map<nx::utils::Url, std::string/*publicIpAddress*/> m_relayIpMockup;
    int m_relayIpHostPart = 0;
    NotifyingRelayClusterClient* m_relayClusterClient = nullptr;

    std::mutex m_mutex;
    std::set<std::string> m_reportedRelayDomains;

    network::http::AsyncClient m_mediatorClientBehindProxy;
    std::promise<network::HostAddress> m_reportedClientHostAddressPromise;
    nx::utils::SyncQueue<std::pair<
        nx::hpm::api::ResultCode,
        nx::hpm::api::ConnectResponse>> m_clientBehindProxyConnectResponse;
};

namespace {

NotifyingRelayClusterClient::NotifyingRelayClusterClient(
    const nx::hpm::conf::Settings& settings,
    nx::geo_ip::AbstractResolver* resolver,
    MultipleRelays* testFixture)
    :
    base_type(settings, resolver),
    m_testFixture(testFixture)
{
}

void NotifyingRelayClusterClient::selectRelayInstanceForListeningPeer(
    const std::string& peerId,
    const nx::network::HostAddress& /*serverHost*/,
    nx::hpm::RelayInstanceSelectCompletionHandler completionHandler)
{
    m_testFixture->addIpAndRegion("127.0.0.1", nx::geo_ip::Continent::northAmerica);

    base_type::selectRelayInstanceForListeningPeer(
        peerId,
        nx::network::HostAddress("127.0.0.1"),
        [this, completionHandler = std::move(completionHandler)](
            nx::cloud::relay::api::ResultCode resultCode,
            std::vector<nx::utils::Url> relayUrls)
        {
            m_testFixture->reportTrafficRelayUrlsForServer(relayUrls);
            completionHandler(resultCode, std::move(relayUrls));
        });
}

void NotifyingRelayClusterClient::findRelayInstanceForClient(
    const std::string& peerId,
    const nx::network::HostAddress& clientHost,
    nx::hpm::RelayInstanceSearchCompletionHandler completionHandler)
{
    // Reporting the actual client host address to verify that http "forwarded*" headers are
    // considered.
    m_testFixture->reportClientHostAddress(clientHost);

    m_testFixture->addIpAndRegion(
        m_testFixture->hardCodedClientIp(),
        nx::geo_ip::Continent::australia);

    base_type::findRelayInstanceForClient(
        peerId,
        m_testFixture->hardCodedClientIp(),
        [this, completionHandler = std::move(completionHandler)](
            nx::cloud::relay::api::ResultCode resultCode,
            nx::utils::Url relayUrl)
        {
            m_testFixture->reportTrafficRelayUrlForClient(relayUrl);
            completionHandler(resultCode, std::move(relayUrl));
        });
}

} // namespace

TEST_F(MultipleRelays, mediator_reports_multiple_traffic_relays_in_same_region_as_server)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
}

TEST_F(MultipleRelays, client_connects_to_server_via_all_relays_reported_to_server)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    thenMultipleRelaysAreReportedToServer();
    thenServerIsListeningOnMultipleRelays();

    thenClientCanConnectToServerViaReportedRelays();
}

TEST_F(MultipleRelays, mediator_reports_traffic_relay_in_same_region_as_client)
{
    // givenMultipleRelays();
    // givenServer();

    whenServerConnectsToMediator();
    whenConnectToMediator();

    thenClientReceivesRelayUrl();
    andClientAndReportedRelayAreInTheSameRegion();
}

TEST_F(MultipleRelays, client_host_address_is_reported_to_mediator_via_x_forwarded_for_header)
{
    whenClientBehindProxyConnectsToMediator(xForwardedForHeader());

    thenMediatorHasCorrectClientHostAddress();

    andConnectRequestBehindProxySucceeds();
}

TEST_F(MultipleRelays, client_host_address_is_reported_to_mediator_via_forwarded_header)
{
    whenClientBehindProxyConnectsToMediator(forwardedHeader());

    thenMediatorHasCorrectClientHostAddress();

    andConnectRequestBehindProxySucceeds();
}

} //namespace nx::network::cloud::test
