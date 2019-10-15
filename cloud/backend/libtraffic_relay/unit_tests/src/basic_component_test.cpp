#include "basic_component_test.h"

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/relay_connection_acceptor.h>
#include <nx/network/url/url_builder.h>

#include <nx/cloud/relay/controller/relay_public_ip_discovery.h>
#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relaying/listening_peer_pool.h>

namespace nx::cloud::relay::test {

template<typename Acceptor>
class AcceptorToServerSocketAdapter:
    public nx::network::StreamServerSocketDelegate
{
    using base_type = nx::network::StreamServerSocketDelegate;

public:
    AcceptorToServerSocketAdapter(std::unique_ptr<Acceptor> acceptor):
        base_type(nullptr),
        m_acceptor(std::move(acceptor))
    {
    }

    virtual nx::network::aio::AbstractAioThread* getAioThread() const override
    {
        return m_acceptor->getAioThread();
    }

    virtual void bindToAioThread(nx::network::aio::AbstractAioThread* aioThread) override
    {
        m_acceptor->bindToAioThread(aioThread);
    }

    virtual void post(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->post(std::move(handler));
    }

    virtual void dispatch(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->dispatch(std::move(handler));
    }

    virtual void pleaseStopSync() override
    {
        m_acceptor->pleaseStopSync();
    }

    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> handler) override
    {
        m_acceptor->pleaseStop(std::move(handler));
    }

    virtual bool getRecvTimeout(unsigned int* millis) const override
    {
        *millis = network::kNoTimeout.count();
        return true;
    }

    virtual void acceptAsync(network::AcceptCompletionHandler handler) override
    {
        m_acceptor->acceptAsync(std::move(handler));
    }

protected:
    virtual void cancelIoInAioThread() override
    {
        m_acceptor->cancelIOSync();
    }

private:
    std::unique_ptr<Acceptor> m_acceptor;
};

//-------------------------------------------------------------------------------------------------

BasicComponentTest::BasicComponentTest(Mode /*mode*/):
    base_type("traffic_relay", QString())
{
    NX_ASSERT(m_discoveryServer.bindAndListen());
    m_relayCluster = std::make_unique<TrafficRelayCluster>(m_discoveryServer.url());
}

void BasicComponentTest::addRelayInstance(
    std::vector<const char*> args,
    bool waitUntilStarted)
{
    auto& relay = m_relayCluster->addRelay();
    for (const auto& arg : args)
        relay.addArg(arg);

    if (waitUntilStarted)
    {
        ASSERT_TRUE(relay.startAndWaitUntilStarted());
    }
    else
    {
        relay.start();
    }
}

Relay& BasicComponentTest::relay(int index)
{
    return m_relayCluster->relay(index);
}

const Relay& BasicComponentTest::relay(int index) const
{
    return m_relayCluster->relay(index);
}

void BasicComponentTest::stopAllInstances()
{
    m_relayCluster->stopAllRelays();
}

bool BasicComponentTest::peerInformationSynchronizedInCluster(
    const std::string& hostname)
{
    return peerInformationSynchronizedInCluster(
        std::vector<std::string>{hostname});
}

bool BasicComponentTest::peerInformationSynchronizedInCluster(
    const std::vector<std::string>& hostnames)
{
    return m_relayCluster->peerInformationSynchronizedInCluster(hostnames);
}

std::unique_ptr<nx::network::http::TestHttpServer> BasicComponentTest::addListeningPeer(
    const std::string& listeningPeerHostName,
    network::ssl::EncryptionUse encryptionUse)
{
    return std::move(addListeningPeers(
        {listeningPeerHostName}, encryptionUse).front());
}

std::vector<std::unique_ptr<nx::network::http::TestHttpServer>>
    BasicComponentTest::addListeningPeers(
        const std::vector<std::string>& listeningPeerHostNames,
        network::ssl::EncryptionUse encryptionUse)
{
    using RelayConnectionAcceptor = nx::network::cloud::relay::ConnectionAcceptor;

    std::vector<std::unique_ptr<nx::network::http::TestHttpServer>> peers;
    for (const auto& hostname: listeningPeerHostNames)
    {
        auto url = relay().httpUrl();
        url.setUserName(hostname.c_str());

        auto acceptor = std::make_unique<RelayConnectionAcceptor>(url);

        std::unique_ptr<nx::network::http::TestHttpServer> peerServer;
        if (encryptionUse == network::ssl::EncryptionUse::always ||
            encryptionUse == network::ssl::EncryptionUse::autoDetectByReceivedData)
        {
            auto sslAcceptor = std::make_unique<network::ssl::StreamServerSocket>(
                std::make_unique<AcceptorToServerSocketAdapter<RelayConnectionAcceptor>>(
                    std::move(acceptor)),
                encryptionUse);
            sslAcceptor->setNonBlockingMode(true);

            peerServer = std::make_unique<nx::network::http::TestHttpServer>(
                std::move(sslAcceptor));
        }
        else if (encryptionUse == network::ssl::EncryptionUse::never)
        {
            peerServer = std::make_unique<nx::network::http::TestHttpServer>(
                std::move(acceptor));
        }

        peerServer->server().start();
        peers.push_back(std::move(peerServer));
    }

    while (!peerInformationSynchronizedInCluster(listeningPeerHostNames))
        std::this_thread::sleep_for(std::chrono::milliseconds(1));

    return peers;
}

} // namespace nx::cloud::relay::test
