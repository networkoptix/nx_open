#include <gtest/gtest.h>

#include <algorithm>
#include "basic_test_fixture.h"
#include "model/abstract_remote_relay_peer_pool.h"
#include <controller/connect_session_manager.h>
#include <controller/../settings.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

using namespace nx::cloud::relay;

class RelayToRelayRedirect;

class MemoryRemoteRelayPeerPool: public model::AbstractRemoteRelayPeerPool
{
public:
    MemoryRemoteRelayPeerPool(const std::string& redirectToRelay, RelayToRelayRedirect* relayTest):
        m_redirectToRelay(redirectToRelay),
        m_relayTest(relayTest)
    {}

    virtual cf::future<std::string> findRelayByDomain(
        const std::string& /*domainName*/) const override
    {
        auto redirectToRelayCopy = m_redirectToRelay;
        return cf::make_ready_future<std::string>(std::move(redirectToRelayCopy));
    }

    virtual cf::future<bool> addPeer(
        const std::string& domainName,
        const std::string& relayHost) override;

    virtual cf::future<bool> removePeer(const std::string& domainName) override;

private:
    std::string m_redirectToRelay;
    RelayToRelayRedirect* m_relayTest;
};

class RelayToRelayRedirect: public BasicTestFixture
{
public:
    RelayToRelayRedirect():
        BasicTestFixture(3)
    {}

    virtual void SetUp() override
    {
        setUpConnectSessionManagerFactoryFunc();
        BasicTestFixture::SetUp();
        ConnectorFactory::setEnabledCloudConnectMask((int)CloudConnectType::proxy);
        startServer();
    }

    void setUpConnectSessionManagerFactoryFunc()
    {
        controller::ConnectSessionManagerFactory::setFactoryFunc(
            [this](
                const conf::Settings& settings,
                model::ClientSessionPool* clientSessionPool,
                model::ListeningPeerPool* listeningPeerPool,
                controller::AbstractTrafficRelay* trafficRelay)
        {
            static int relayPort = 20000;

            return std::make_unique<controller::ConnectSessionManager>(
                settings,
                clientSessionPool,
                listeningPeerPool,
                trafficRelay,
                std::make_unique<MemoryRemoteRelayPeerPool>("127.0.0.1:20000", this),
                std::move("127.0.0.1:" + std::to_string(relayPort++)));
        });
    }

    void assertServerIsAccessibleFromAnotherRelay()
    {
        using namespace nx::cloud::relay;

        auto relayClient = api::ClientFactory::create(relayUrl(2));

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
            if (std::get<0>(result) == api::ResultCode::ok)
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            qWarning() << "waiting...";
        }
    }

    void assertFirstRelayHasBeenAddedToThePool()
    {
        auto it = std::find_if(
            m_addedPeers.cbegin(),
            m_addedPeers.cend(),
            [this](const std::pair<std::string, std::string>& p)
        {
            return
                nx::String::fromStdString(p.first).contains(this->serverSocketCloudAddress())
                && p.second == "127.0.0.1:20000";
        });

        ASSERT_NE(it, m_addedPeers.cend());
    }

    void addPeerRelay(const std::string& domainName, const std::string& relayHost)
    {
        m_addedPeers.push_back(std::make_pair(domainName, relayHost));
    }

    void removePeer(const std::string& domainName)
    {
        m_removedPeer = domainName;
    }

    void whenServerGoesOffline()
    {
        stopServer();
    }

    void thenItsRecordShouldBeRemovedFromRemoteRelayPool()
    {
        nx::String removedPeer = nx::String::fromStdString(m_removedPeer);
        nx::String firstRelayServerId = this->serverSocketCloudAddress();

        ASSERT_TRUE(removedPeer.contains(firstRelayServerId));
    }

private:
    std::vector<std::pair<std::string, std::string>> m_addedPeers;
    std::string m_removedPeer;
};

cf::future<bool> MemoryRemoteRelayPeerPool::addPeer(
    const std::string& domainName,
    const std::string& relayHost)
{
    m_relayTest->addPeerRelay(domainName, relayHost);
    return cf::make_ready_future(true);
}

cf::future<bool> MemoryRemoteRelayPeerPool::removePeer(const std::string& domainName)
{
    m_relayTest->removePeer(domainName);
    return cf::make_ready_future(true);
}

TEST_F(RelayToRelayRedirect, RedirectToAnotherRelay)
{
    assertServerIsAccessibleFromAnotherRelay();
    assertFirstRelayHasBeenAddedToThePool();
}

TEST_F(RelayToRelayRedirect, RecordRemovedOnServerDisconnect)
{
    whenServerGoesOffline();
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    thenItsRecordShouldBeRemovedFromRemoteRelayPool();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx