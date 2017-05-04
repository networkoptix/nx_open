#include <gtest/gtest.h>

#include <nx/network/http/test_http_server.h>
#include <nx/utils/log/log.h>
#include <nx/utils/test_support/sync_queue.h>
#include <nx/vms/discovery/module_connector.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace discovery {
namespace test {

class DiscoveryModuleConnector: public testing::Test
{
public:
    DiscoveryModuleConnector()
    {
        connector.setReconnectInterval(std::chrono::seconds(1));
        connector.setConnectHandler(connectedQueue.pusher());
        connector.setDisconnectHandler(disconnectedQueue.pusher());
        connector.activate();
    }

    ~DiscoveryModuleConnector()
    {
        connector.pleaseStopSync();
    }

    void expectConnect(const QnUuid& id, const SocketAddress& endpoint)
    {
        const auto server = connectedQueue.pop();
        EXPECT_EQ(id, server.first.id);
        EXPECT_EQ(endpoint.toString(), server.second.toString()) << id.toStdString();
    }

    SocketAddress addMediaserver(QnUuid id, HostAddress ip = HostAddress::localhost)
    {
        QnModuleInformation module;
        module.id = std::move(id);

        QnJsonRestResult result;
        result.setReply(module);

        auto server = std::make_unique<TestHttpServer>();
        const bool registration = server->registerStaticProcessor(
            QLatin1String("/api/moduleInformation"), QJson::serialized(result),
            Qn::serializationFormatToHttpContentType(Qn::JsonFormat));

        EXPECT_TRUE(registration);
        EXPECT_TRUE(server->bindAndListen(ip));

        const auto endpoint = server->serverAddress();
        mediaservers[endpoint] = std::move(server);
        return endpoint;
    }

    ModuleConnector connector;
    utils::TestSyncMultiQueue<QnModuleInformation, SocketAddress> connectedQueue;
    utils::TestSyncQueue<QnUuid> disconnectedQueue;
    std::map<SocketAddress, std::unique_ptr<TestHttpServer>> mediaservers;
};

TEST_F(DiscoveryModuleConnector, AddEndpoints)
{
    const auto id1 = QnUuid::createUuid();
    const auto endpoint1 = addMediaserver(id1);
    connector.newEndpoint(endpoint1, id1);
    expectConnect(id1, endpoint1);

    const auto id2 = QnUuid::createUuid();
    const auto endpoint2 = addMediaserver(id2);
    connector.newEndpoint(endpoint2);
    expectConnect(id2, endpoint2);

    const auto endpoint1replace = addMediaserver(id1);
    connector.newEndpoint(endpoint1replace, id1);
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
    connector.diactivate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    const auto id = QnUuid::createUuid();
    const auto endpoint1 = addMediaserver(id);
    connector.newEndpoint(endpoint1);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty());
    ASSERT_TRUE(disconnectedQueue.isEmpty());

    connector.activate();
    expectConnect(id, endpoint1);

    connector.diactivate();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mediaservers.erase(endpoint1);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Disconnect is reported anyway.

    const auto endpoint2 = addMediaserver(id);
    connector.newEndpoint(endpoint2, id);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    ASSERT_TRUE(connectedQueue.isEmpty()); //< No connects on disabled connector.
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Disconnect is reported anyway.

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
    connector.diactivate();

    const auto networkEndpoint = addMediaserver(id, *interfaceIpsV4.begin());
    const auto localEndpoint = addMediaserver(id);
    connector.newEndpoint(networkEndpoint, id);
    connector.newEndpoint(localEndpoint, id);

    connector.activate();
    expectConnect(id, localEndpoint); //< Local is prioritized.

    mediaservers.erase(localEndpoint);
    connector.newEndpoint(networkEndpoint, id); //< Network is a second choice.

    mediaservers.erase(networkEndpoint);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Finaly no endpoint avaliable.
}

TEST_F(DiscoveryModuleConnector, IgnoredEndpoints)
{
    const auto id = QnUuid::createUuid();
    connector.diactivate();

    const auto endpoint1 = addMediaserver(id);
    connector.newEndpoint(endpoint1, id);
    const auto endpoint2 = addMediaserver(id);
    connector.newEndpoint(endpoint2, id);
    const auto endpoint3 = addMediaserver(id);
    connector.newEndpoint(endpoint3, id);

    connector.ignoreEndpoints({endpoint1, endpoint3}, id);
    connector.activate();
    expectConnect(id, endpoint2); //< The only one which is not blocked.

    connector.ignoreEndpoints({endpoint3}, id);
    mediaservers.erase(endpoint2);
    expectConnect(id, endpoint1); //< Another one is unblocked now.

    mediaservers.erase(endpoint1);
    ASSERT_EQ(id, disconnectedQueue.pop()); //< Ony blocked endpoint is reachable.

    connector.ignoreEndpoints({}, id);
    expectConnect(id, endpoint3); //< The last one is unblocked now.
}

} // namespace test
} // namespace discovery
} // namespace vms
} // namespace nx
