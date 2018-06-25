#include <algorithm>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/connector_factory.h>
#include <nx/network/cloud/tunnel/relay/api/relay_api_client_factory.h>
#include <nx/utils/std/future.h>

#include <nx/cloud/relay/settings.h>

#include "basic_test_fixture.h"

namespace nx {
namespace network {
namespace cloud {
namespace test {

using namespace nx::cloud::relay;

class RelayToRelayRedirect;

class RelayToRelayRedirect: public BasicTestFixture
{
public:
    RelayToRelayRedirect():
        BasicTestFixture(3, std::chrono::seconds(1)),
        m_removePeerFuture(m_removePeerPromise.get_future()),
        m_addPeerFuture(m_addPeerPromise.get_future())
    {}

    virtual void SetUp() override
    {
        BasicTestFixture::SetUp();
        ConnectorFactory::setEnabledCloudConnectMask((int)ConnectType::proxy);
        startServer();
    }

    void assertServerIsAccessibleFromAnotherRelay()
    {
        using namespace nx::cloud::relay;

        auto relayClient = api::ClientFactory::instance().create(relayUrl(2));

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
        ASSERT_TRUE(m_addedPeer.find(serverSocketCloudAddress()) != std::string::npos);
    }

    virtual void peerAdded(const std::string& domainName) override
    {
        m_addedPeer = domainName;
        m_addPeerPromise.set_value();
    }

    virtual void peerRemoved(const std::string& domainName) override
    {
        m_removedPeer = domainName;
        m_removePeerPromise.set_value();
    }

    void whenServerGoesOffline()
    {
        stopServer();
    }

    void thenItsRecordShouldBeRemovedFromRemoteRelayPool()
    {
        ASSERT_TRUE(m_removedPeer.find(this->serverSocketCloudAddress()) != std::string::npos);
    }

    void waitForRemovePeerSignal()
    {
        m_removePeerFuture.wait();
    }

    void waitForAddPeerSignal()
    {
        m_addPeerFuture.wait();
    }


private:
    std::string m_addedPeer;
    std::string m_removedPeer;

    nx::utils::promise<void> m_removePeerPromise;
    nx::utils::future<void> m_removePeerFuture;

    nx::utils::promise<void> m_addPeerPromise;
    nx::utils::future<void> m_addPeerFuture;
};

TEST_F(RelayToRelayRedirect, RedirectToAnotherRelay)
{
    assertServerIsAccessibleFromAnotherRelay();
    waitForAddPeerSignal();
    assertFirstRelayHasBeenAddedToThePool();
}

TEST_F(RelayToRelayRedirect, RecordRemovedOnServerDisconnect)
{
    whenServerGoesOffline();
    waitForRemovePeerSignal();
    thenItsRecordShouldBeRemovedFromRemoteRelayPool();
}

} // namespace test
} // namespace cloud
} // namespace network
} // namespace nx
