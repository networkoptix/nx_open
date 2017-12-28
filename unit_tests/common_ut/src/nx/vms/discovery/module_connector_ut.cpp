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

static const HostAddress kLocalDnsHost("local-doman-name.com");
static const HostAddress kLocalCloudHost(QnUuid::createUuid().toSimpleString());

static const std::chrono::seconds kMaxWaitDelay(30);
static const std::chrono::milliseconds kExpectNoChanesDelay(300);
static const network::RetryPolicy kReconnectPolicy(
    network::RetryPolicy::kInfiniteRetries,
    std::chrono::milliseconds(100), 2, std::chrono::seconds(1));

class DiscoveryModuleConnector: public testing::Test
{
public:
    DiscoveryModuleConnector()
    {
        connector.setDisconnectTimeout(std::chrono::seconds(1));
        connector.setReconnectPolicy(kReconnectPolicy);

        connector.setConnectHandler(
            [this](QnModuleInformation information, SocketAddress endpoint, HostAddress /*ip*/)
            {
                QnMutexLocker lock(&m_mutex);
                auto& knownEndpoint = m_knownServers[information.id];
                if (knownEndpoint == endpoint)
                    return;

                NX_INFO(this, lm("Server %1 is found on %2").args(information.id, endpoint));
                knownEndpoint = std::move(endpoint);
                m_condition.wakeAll();
            });

        connector.setDisconnectHandler(
            [this](QnUuid id)
            {
                QnMutexLocker lock(&m_mutex);
                const auto iterator = m_knownServers.find(id);
                if (iterator == m_knownServers.end())
                    return;

                NX_INFO(this, lm("Server %1 is lost").arg(id));
                m_knownServers.erase(iterator);
                m_condition.wakeAll();
            });

        connector.activate();

        auto& dnsResolver = network::SocketGlobals::addressResolver().dnsResolver();
        dnsResolver.addEtcHost(kLocalDnsHost.toString(), {HostAddress::localhost});
        dnsResolver.addEtcHost(kLocalCloudHost.toString(), {HostAddress::localhost});
    }

    ~DiscoveryModuleConnector()
    {
        connector.pleaseStopSync();

        auto& dnsResolver = network::SocketGlobals::addressResolver().dnsResolver();
        dnsResolver.removeEtcHost(kLocalDnsHost.toString());
        dnsResolver.removeEtcHost(kLocalCloudHost.toString());
    }

    void expectConnect(const QnUuid& id, const SocketAddress& endpoint)
    {
        QnMutexLocker lock(&m_mutex);
        const auto start = std::chrono::steady_clock::now();
        while (m_knownServers.count(id) == 0 || m_knownServers[id].toString() != endpoint.toString())
        {
            ASSERT_TRUE(waitCondition(start)) << lm("Server %1 is not found on %2 within timeout")
                .args(id, endpoint).toStdString();
        }
    }

    void expectDisconnect(const QnUuid& id)
    {
        QnMutexLocker lock(&m_mutex);
        const auto start = std::chrono::steady_clock::now();
        while (m_knownServers.count(id) != 0)
        {
            ASSERT_TRUE(waitCondition(start)) << lm("Server %1 is not lost within timeout")
                .args(id).toStdString();
        }
    }

    void expectNoChanges()
    {
        QnMutexLocker lock(&m_mutex);
        ASSERT_FALSE(waitCondition(kExpectNoChanesDelay)) << "Unexpected event";
    }

    SocketAddress addMediaserver(QnUuid id, HostAddress ip = HostAddress::localhost)
    {
        QnModuleInformation module;
        module.id = id;

        QnJsonRestResult result;
        result.setReply(module);

        auto server = std::make_unique<TestHttpServer>();
        const bool registration = server->registerStaticProcessor(
            QLatin1String("/api/moduleInformation"), QJson::serialized(result),
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

        EXPECT_TRUE(registration);
        EXPECT_TRUE(server->bindAndListen(ip));

        const auto endpoint = server->serverAddress();
        NX_INFO(this, lm("Server %1 is added on %2").args(id, endpoint));
        m_mediaservers[endpoint] = std::move(server);
        return endpoint;
    }

    void removeMediaserver(const SocketAddress& endpoint)
    {
        EXPECT_GT(m_mediaservers.erase(endpoint), 0);
        NX_INFO(this, lm("Server is removed on %2").args(endpoint));
    }

private:
    bool waitCondition(std::chrono::steady_clock::time_point start)
    {
        const auto timeLeft = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start) + kMaxWaitDelay;

        return timeLeft.count() > 0 && waitCondition(timeLeft);
    }

    bool waitCondition(std::chrono::milliseconds duration)
    {
        return m_condition.wait(&m_mutex, duration.count());
    }

protected:
    ModuleConnector connector;

private:
    std::map<SocketAddress, std::unique_ptr<TestHttpServer>> m_mediaservers;

    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::map<QnUuid, SocketAddress> m_knownServers;
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
    expectNoChanges();

    removeMediaserver(endpoint1);
    expectConnect(id1, endpoint1replace);

    removeMediaserver(endpoint2);
    expectDisconnect(id2);
}

TEST_F(DiscoveryModuleConnector, ActivateDiactivate)
{
    connector.deactivate();
    std::this_thread::sleep_for(kExpectNoChanesDelay);

    const auto id = QnUuid::createUuid();
    const auto endpoint1 = addMediaserver(id);
    connector.newEndpoints({endpoint1});
    expectNoChanges();

    connector.activate();
    expectConnect(id, endpoint1);

    connector.deactivate();
    std::this_thread::sleep_for(kExpectNoChanesDelay);

    removeMediaserver(endpoint1);
    expectDisconnect(id); //< Disconnect is reported anyway.

    const auto endpoint2 = addMediaserver(id);
    connector.newEndpoints({endpoint2}, id);
    expectNoChanges(); //< No connects on disabled connector.

    connector.activate();
    expectConnect(id, endpoint2); //< Connect right after reactivate.
}

TEST_F(DiscoveryModuleConnector, EndpointPriority)
{
    HostAddress interfaceIp;
    const auto interfaceIpsV4 = getLocalIpV4AddressList();
    if (interfaceIpsV4.empty())
        return;

    const auto id = QnUuid::createUuid();
    connector.deactivate();

    const auto networkEndpoint = addMediaserver(id, *interfaceIpsV4.begin());
    const auto localEndpoint = addMediaserver(id);
    connector.newEndpoints({localEndpoint, networkEndpoint}, id);

    connector.activate();
    expectConnect(id, localEndpoint); //< Local is prioritized.

    removeMediaserver(localEndpoint);
    expectConnect(id, networkEndpoint);  //< Network is a second choice.

    const auto newLocalEndpoint = addMediaserver(id);
    connector.newEndpoints({newLocalEndpoint}, id);
    expectConnect(id, newLocalEndpoint);  //< Expected switch to a new local endpoint.

    const auto dnsRealEndpoint = addMediaserver(id);
    const auto cloudRealEndpoint = addMediaserver(id);
    const SocketAddress dnsEndpoint(kLocalDnsHost, dnsRealEndpoint.port);
    const SocketAddress cloudEndpoint(kLocalCloudHost, cloudRealEndpoint.port);
    connector.newEndpoints({dnsEndpoint, cloudEndpoint}, id);

    removeMediaserver(networkEndpoint);
    expectNoChanges(); // Not any reconnects are expected.

    removeMediaserver(newLocalEndpoint);
    expectConnect(id, dnsEndpoint);  //< DNS is the most prioritized now.

    removeMediaserver(dnsRealEndpoint);
    expectConnect(id, cloudEndpoint);  //< Cloud endpoint is the last possible option.

    const auto newDnsRealEndpoint = addMediaserver(id);
    const SocketAddress newDnsEndpoint(kLocalDnsHost, newDnsRealEndpoint.port);
    connector.newEndpoints({newDnsEndpoint}, id);
    expectConnect(id, newDnsEndpoint);  //< Expected switch to DNS as better choise than cloud.

    removeMediaserver(cloudRealEndpoint);
    removeMediaserver(newDnsRealEndpoint);
    expectDisconnect(id); //< Finally no endpoints avaliable.
}

TEST_F(DiscoveryModuleConnector, IgnoredEndpoints)
{
    const auto id = QnUuid::createUuid();
    connector.deactivate();
    std::this_thread::sleep_for(kExpectNoChanesDelay);

    const auto endpoint1 = addMediaserver(id);
    const auto endpoint2 = addMediaserver(id);
    const auto endpoint3 = addMediaserver(id);
    connector.newEndpoints({endpoint1, endpoint2, endpoint3}, id);

    connector.setForbiddenEndpoints({endpoint1, endpoint3}, id);
    connector.activate();
    expectConnect(id, endpoint2); //< The only one which is not blocked.

    connector.setForbiddenEndpoints({endpoint3}, id);
    removeMediaserver(endpoint2);
    expectConnect(id, endpoint1); //< Another one is unblocked now.

    removeMediaserver(endpoint1);
    expectDisconnect(id); //< Only blocked endpoints are reachable.

    connector.setForbiddenEndpoints({}, id);
    expectConnect(id, endpoint3); //< The last one is unblocked now.
}

// This unit test is just for easy debug agains real mediaserver.
TEST_F(DiscoveryModuleConnector, DISABLED_RealLocalServer)
{
    connector.newEndpoints({ SocketAddress("127.0.0.1:7001") }, QnUuid());
    std::this_thread::sleep_for(std::chrono::hours(1));
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
