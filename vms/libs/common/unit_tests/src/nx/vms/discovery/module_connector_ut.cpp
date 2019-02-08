#include <gtest/gtest.h>

#include <nx/network/app_info.h>
#include <nx/network/address_resolver.h>
#include <nx/network/cloud/cloud_connect_controller.h>
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
            [this](nx::vms::api::ModuleInformation information,
                nx::network::SocketAddress endpoint, nx::network::SocketAddress /*resolved*/)
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
    }

    ~DiscoveryModuleConnector()
    {
        connector.pleaseStopSync();
    }

    void expectConnect(const QnUuid& id, const nx::network::SocketAddress& endpoint)
    {
        NX_INFO(this, lm("Excpecting connect to %1 on %2").args(id, endpoint));

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
        NX_INFO(this, lm("Excpecting disconnect of %1").arg(id));

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
        NX_INFO(this, "Excpecting no changes");

        QnMutexLocker lock(&m_mutex);
        ASSERT_FALSE(waitCondition(kExpectNoChanesDelay)) << "Unexpected event";
    }

    void expectPossibleChange()
    {
        NX_INFO(this, "Excpecting possible address change");

        QnMutexLocker lock(&m_mutex);
        waitCondition(kExpectNoChanesDelay);
    }

    nx::network::SocketAddress addMediaserver(QnUuid id, nx::network::HostAddress ip = nx::network::HostAddress::localhost)
    {
        nx::vms::api::ModuleInformation module;
        module.realm = nx::network::AppInfo::realm();
        module.cloudHost = nx::network::SocketGlobals::cloud().cloudHost();
        module.id = id;

        QnJsonRestResult result;
        result.setReply(module);

        auto server = std::make_unique<nx::network::http::TestHttpServer>();
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

    void removeMediaserver(const nx::network::SocketAddress& endpoint)
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
    std::map<nx::network::SocketAddress, std::unique_ptr<nx::network::http::TestHttpServer>> m_mediaservers;

    QnMutex m_mutex;
    QnWaitCondition m_condition;
    std::map<QnUuid, nx::network::SocketAddress> m_knownServers;
};

struct DnsAlias
{
    const nx::network::SocketAddress original;
    const nx::network::SocketAddress alias;

    DnsAlias(nx::network::SocketAddress original_, nx::network::HostAddress alias_):
        original(original_),
        alias(alias_, original_.port)
    {
        nx::network::SocketGlobals::addressResolver().addFixedAddress(alias.address, original);
    }

    ~DnsAlias()
    {
        nx::network::SocketGlobals::addressResolver().removeFixedAddress(alias.address, original);
    }

    DnsAlias(const DnsAlias&) = delete;
    DnsAlias& operator=(const DnsAlias&) = delete;
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
    expectPossibleChange(); //< Endpoint will be changed if works faster.

    removeMediaserver(endpoint1);
    expectConnect(id1, endpoint1replace);

    removeMediaserver(endpoint2);
    expectDisconnect(id2);
}

TEST_F(DiscoveryModuleConnector, forgetModule)
{
    const auto id = QnUuid::createUuid();
    const auto endpoint = addMediaserver(id);
    connector.newEndpoints({endpoint});
    expectConnect(id, endpoint);
    connector.forgetModule(id);
    expectDisconnect(id);
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

    removeMediaserver(localEndpoint);
    expectConnect(id, networkEndpoint); //< Network is a second choice.

    const auto newLocalEndpoint = addMediaserver(id);
    connector.newEndpoints({newLocalEndpoint}, id);
    expectConnect(id, newLocalEndpoint); //< New local is prioritized.

    const DnsAlias dnsEndpoint(addMediaserver(id), "local-doman-name.com");
    const DnsAlias cloudEndpoint(addMediaserver(id), QnUuid::createUuid().toSimpleString());

    connector.newEndpoints({dnsEndpoint.alias, cloudEndpoint.alias}, id);
    expectConnect(id, dnsEndpoint.alias);  //< DNS is the most prioritized.

    removeMediaserver(newLocalEndpoint);
    removeMediaserver(networkEndpoint);
    removeMediaserver(dnsEndpoint.original);
    expectConnect(id, cloudEndpoint.alias);  //< Cloud endpoint is the last possible option.

    const DnsAlias newDnsEndpoint(addMediaserver(id), "local-doman-name.com");
    connector.newEndpoints({newDnsEndpoint.alias}, id);
    expectConnect(id, newDnsEndpoint.alias);

    removeMediaserver(cloudEndpoint.original);
    removeMediaserver(newDnsEndpoint.original);
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

    const auto endpoint4 = addMediaserver(id);
    connector.newEndpoints({endpoint4}, id);
    connector.setForbiddenEndpoints({endpoint3}, id);
    expectConnect(id, endpoint4); //< Automatic switch from blocked endpoint.
}

// This unit test is just for easy debug agains real mediaserver.
TEST_F(DiscoveryModuleConnector, DISABLED_RealLocalServer)
{
    connector.newEndpoints({ nx::network::SocketAddress("127.0.0.1:7001") }, QnUuid());
    std::this_thread::sleep_for(std::chrono::hours(1));
}

TEST_F(DiscoveryModuleConnector, IgnoredEndpointsByStrings)
{
    const auto firstId = QnUuid::createUuid();
    const auto firstEndpoint1 = addMediaserver(firstId);
    const auto firstEndpoint2 = addMediaserver(firstId);

    connector.newEndpoints({firstEndpoint1}, firstId);
    expectConnect(firstId, firstEndpoint1); //< Only have one.

    connector.newEndpoints({firstEndpoint2}, firstId);
    connector.setForbiddenEndpoints({firstEndpoint1.toString()}, firstId);
    expectConnect(firstId, firstEndpoint2); //< Switch to a single avaliable.

    const auto secondId = QnUuid::createUuid();
    const auto secondEndpoint1 = addMediaserver(secondId);
    const auto secondEndpoint2 = addMediaserver(secondId);

    connector.newEndpoints({secondEndpoint1});
    expectConnect(secondId, secondEndpoint1); //< Detect correct serverId.

    connector.newEndpoints({secondEndpoint2}, secondId);
    connector.setForbiddenEndpoints({secondEndpoint1.toString()}, secondId);
    expectConnect(secondId, secondEndpoint2); //< Switch to a single avaliable.
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
