#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/http/test_http_server.h>
#include <nx/network/nettools.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/vms/discovery/module_connector.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {
namespace test {

static const nx::network::HostAddress kLocalDnsHost("local-doman-name.com");
static const nx::network::HostAddress kLocalCloudHost(QnUuid::createUuid().toSimpleString());

class DiscoveryModuleConnector: public testing::Test
{
public:
    DiscoveryModuleConnector()
    {
        connector.setDisconnectTimeout(std::chrono::seconds(1));
        connector.setReconnectPolicy(network::RetryPolicy(network::RetryPolicy::kInfiniteRetries,
            std::chrono::milliseconds(100), 2, std::chrono::seconds(1)));

        connector.setConnectHandler(
            [this](QnModuleInformation information, nx::network::SocketAddress endpoint, nx::network::HostAddress /*ip*/)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    auto& knownEndpoint = m_knownServers[information.id];
                    if (knownEndpoint == endpoint)
                        return;

                    knownEndpoint = endpoint;
                }

                connectedQueue.push(std::move(information.id), std::move(endpoint));
            });

        connector.setDisconnectHandler(
            [this](QnUuid id)
            {
                {
                    QnMutexLocker lock(&m_mutex);
                    if (m_knownServers.erase(id) == 0)
                        return;
                }

                disconnectedQueue.push(std::move(id));
            });

        connector.activate();

        auto& dnsResolver = network::SocketGlobals::addressResolver().dnsResolver();
        dnsResolver.addEtcHost(kLocalDnsHost.toString(), {nx::network::HostAddress::localhost});
        dnsResolver.addEtcHost(kLocalCloudHost.toString(), {nx::network::HostAddress::localhost});
    }

    ~DiscoveryModuleConnector()
    {
        connector.pleaseStopSync();

        auto& dnsResolver = network::SocketGlobals::addressResolver().dnsResolver();
        dnsResolver.removeEtcHost(kLocalDnsHost.toString());
        dnsResolver.removeEtcHost(kLocalCloudHost.toString());

        EXPECT_TRUE(connectedQueue.isEmpty());
        EXPECT_TRUE(disconnectedQueue.isEmpty());
    }

    void expectConnect(const QnUuid& id, const nx::network::SocketAddress& endpoint)
    {
        const auto server = connectedQueue.pop();
        EXPECT_EQ(id.toString(), server.first.toString());
        EXPECT_EQ(endpoint.toString(), server.second.toString()) << id.toStdString();
    }

    nx::network::SocketAddress addMediaserver(QnUuid id, nx::network::HostAddress ip = nx::network::HostAddress::localhost)
    {
        QnModuleInformation module;
        module.id = std::move(id);

        QnJsonRestResult result;
        result.setReply(module);

        auto server = std::make_unique<nx::network::http::TestHttpServer>();
        const bool registration = server->registerStaticProcessor(
            QLatin1String("/api/moduleInformation"), QJson::serialized(result),
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

        EXPECT_TRUE(registration);
        EXPECT_TRUE(server->bindAndListen(ip));

        const auto endpoint = server->serverAddress();
        mediaservers[endpoint] = std::move(server);
        NX_INFO(this, lm("Module %1 started with endpoint %2").args(module.id, endpoint));
        return endpoint;
    }

    QnMutex m_mutex;
    std::map<QnUuid, nx::network::SocketAddress> m_knownServers;

    ModuleConnector connector;
    utils::TestSyncMultiQueue<QnUuid, nx::network::SocketAddress> connectedQueue;
    utils::TestSyncQueue<QnUuid> disconnectedQueue;
    std::map<nx::network::SocketAddress, std::unique_ptr<nx::network::http::TestHttpServer>> mediaservers;
};

TEST_F(DiscoveryModuleConnector, AddEndpoints)
{
    const auto id1 = QnUuid::createUuid();
    const auto endpoint1 = addMediaserver(id1);
    connector.newEndpoints({endpoint1}, id1);
    expectConnect(id1, endpoint1);

    const auto id2 = QnUuid::createUuid();
    const auto endpoint2 = addMediaserver(id2);
    connector.newEndpoints({endpoint2});
    expectConnect(id2, endpoint2);

    const auto endpoint1replace = addMediaserver(id1);
    connector.newEndpoints({endpoint1replace}, id1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty());
    ASSERT_TRUE(disconnectedQueue.isEmpty());

    mediaservers.erase(endpoint1);
    expectConnect(id1, endpoint1replace);

    mediaservers.erase(endpoint2);
    ASSERT_EQ(id2, disconnectedQueue.pop());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty());
    ASSERT_TRUE(disconnectedQueue.isEmpty());
}

TEST_F(DiscoveryModuleConnector, ActivateDiactivate)
{
    connector.deactivate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto id = QnUuid::createUuid();
    const auto endpoint1 = addMediaserver(id);
    connector.newEndpoints({endpoint1});
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty());
    ASSERT_TRUE(disconnectedQueue.isEmpty());

    connector.activate();
    expectConnect(id, endpoint1);

    connector.deactivate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mediaservers.erase(endpoint1);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Disconnect is reported anyway.

    const auto endpoint2 = addMediaserver(id);
    connector.newEndpoints({endpoint2}, id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty()); //< No connects on disabled connector.

    connector.activate();
    expectConnect(id, endpoint2); //< Connect right after reactivate.
}

TEST_F(DiscoveryModuleConnector, EndpointPriority)
{
    nx::network::HostAddress interfaceIp;
    const auto interfaceIpsV4 = nx::network::getLocalIpV4AddressList();
    if (interfaceIpsV4.empty())
        return;

    const auto id = QnUuid::createUuid();
    connector.deactivate();

    const auto networkEndpoint = addMediaserver(id, *interfaceIpsV4.begin());
    const auto localEndpoint = addMediaserver(id);
    connector.newEndpoints({localEndpoint, networkEndpoint}, id);

    connector.activate();
    expectConnect(id, localEndpoint); //< Local is prioritized.

    mediaservers.erase(localEndpoint);
    expectConnect(id, networkEndpoint);  //< Network is a second choice.

    const auto newLocalEndpoint = addMediaserver(id);
    connector.newEndpoints({newLocalEndpoint}, id);
    expectConnect(id, newLocalEndpoint);  //< Expected switch to a new local endpoint.

    const auto dnsRealEndpoint = addMediaserver(id);
    const auto cloudRealEndpoint = addMediaserver(id);
    const nx::network::SocketAddress dnsEndpoint(kLocalDnsHost, dnsRealEndpoint.port);
    const nx::network::SocketAddress cloudEndpoint(kLocalCloudHost, cloudRealEndpoint.port);
    connector.newEndpoints({dnsEndpoint, cloudEndpoint}, id);

    mediaservers.erase(networkEndpoint);
    std::this_thread::sleep_for(std::chrono::seconds(1));
    ASSERT_TRUE(connectedQueue.isEmpty()); // Not any reconnects are expected.
    ASSERT_TRUE(disconnectedQueue.isEmpty());

    mediaservers.erase(newLocalEndpoint);
    expectConnect(id, dnsEndpoint);  //< DNS is the most prioritized now.

    mediaservers.erase(dnsRealEndpoint);
    expectConnect(id, cloudEndpoint);  //< Cloud endpoint is the last possible option.

    const auto newDnsRealEndpoint = addMediaserver(id);
    const nx::network::SocketAddress newDnsEndpoint(kLocalDnsHost, newDnsRealEndpoint.port);
    connector.newEndpoints({newDnsEndpoint}, id);
    expectConnect(id, newDnsEndpoint);  //< Expected switch to DNS as better choise than cloud.

    mediaservers.erase(cloudRealEndpoint);
    mediaservers.erase(newDnsRealEndpoint);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Finally no endpoints avaliable.
}

TEST_F(DiscoveryModuleConnector, IgnoredEndpoints)
{
    const auto id = QnUuid::createUuid();
    connector.deactivate();

    const auto endpoint1 = addMediaserver(id);
    const auto endpoint2 = addMediaserver(id);
    const auto endpoint3 = addMediaserver(id);
    connector.newEndpoints({endpoint1, endpoint2, endpoint3}, id);

    connector.setForbiddenEndpoints({endpoint1, endpoint3}, id);
    connector.activate();
    expectConnect(id, endpoint2); //< The only one which is not blocked.

    connector.setForbiddenEndpoints({endpoint3}, id);
    mediaservers.erase(endpoint2);
    expectConnect(id, endpoint1); //< Another one is unblocked now.

    mediaservers.erase(endpoint1);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Only blocked endpoints is reachable.

    connector.setForbiddenEndpoints({}, id);
    expectConnect(id, endpoint3); //< The last one is unblocked now.
}

// This unit test is just for easy debug agains real mediaserver.
TEST_F(DiscoveryModuleConnector, DISABLED_RealLocalServer)
{
    connector.newEndpoints({nx::network::SocketAddress("127.0.0.1:7001")}, QnUuid());
    std::this_thread::sleep_for(std::chrono::hours(1));
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
