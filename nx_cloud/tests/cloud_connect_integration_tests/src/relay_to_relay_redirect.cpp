#include <gtest/gtest.h>

#include "basic_test_fixture.h"
#include "model/abstract_remote_relay_peer_pool.h"
#include <controller/connect_session_manager.h>
#include <controller/../settings.h>

namespace nx {
namespace network {
namespace cloud {
namespace test {

using namespace nx::cloud::relay;

class MemoryRemoteRelayPeerPool: public model::AbstractRemoteRelayPeerPool
{
public:
    MemoryRemoteRelayPeerPool(const std::string& redirectToRelay):
        m_redirectToRelay(redirectToRelay)
    {}

    virtual cf::future<std::string> findRelayByDomain(const std::string& domainName) const override
    {
        auto redirectToRelayCopy = m_redirectToRelay;
        qWarning() << "findRelayByDomain: returning" << QString::fromStdString(redirectToRelayCopy);
        return cf::make_ready_future<std::string>(std::move(redirectToRelayCopy));
    }

    virtual cf::future<bool> addPeer(
        const std::string& domainName,
        const std::string& relayHost) override
    {
        return cf::make_ready_future(true);
    }

    virtual cf::future<bool> removePeer(const std::string& domainName) override
    {
        return cf::make_ready_future(false);
    }

private:
    std::string m_redirectToRelay;
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
            [](
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
                std::make_unique<MemoryRemoteRelayPeerPool>("127.0.0.1:20000"),
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
};

TEST_F(RelayToRelayRedirect, RedirectToAnotherRelay)
{
    assertServerIsAccessibleFromAnotherRelay();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx