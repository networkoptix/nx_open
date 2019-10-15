#include <array>
#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <nx/network/cloud/tunnel/relay/api/relay_api_client.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/http_request_generator.h>
#include <nx/utils/random.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/std/optional.h>
#include <nx/utils/string.h>
#include <nx/utils/thread/sync_queue.h>

#include <nx/cloud/relay/model/remote_relay_peer_pool.h>
#include <nx/cloud/relay/settings.h>

#include "basic_component_test.h"

namespace nx {
namespace cloud {
namespace relay {
namespace test {

namespace {

class TestRemoteRelayPeerPool:
    public model::RemoteRelayPeerPool
{
    using base_type = model::RemoteRelayPeerPool;

public:
    TestRemoteRelayPeerPool(
        const conf::Settings& settings,
        nx::utils::SyncQueue<bool>* initEvents,
        const bool* const forceConnectionFailure)
        :
        base_type(settings),
        m_initEvents(initEvents),
        m_forceConnectionFailure(forceConnectionFailure)
    {
    }

    virtual bool connectToDb() override
    {
        if (*m_forceConnectionFailure)
        {
            m_initEvents->push(false);
            return false;
        }

        bool connected = base_type::connectToDb();
        m_initEvents->push(connected);
        return connected;
    }

    virtual bool isConnected() const override
    {
        if (*m_forceConnectionFailure)
            return false;

        return base_type::isConnected();
    }

private:
    nx::utils::SyncQueue<bool>* m_initEvents;
    const bool* const m_forceConnectionFailure = nullptr;
};

} // namespace

//-------------------------------------------------------------------------------------------------

class ListeningPeerRequestGenerator:
    public nx::network::test::RequestGenerator
{
    using base_type = nx::network::test::RequestGenerator;

public:
    ListeningPeerRequestGenerator(
        std::size_t maxSimultaneousRequestCount,
        const nx::utils::Url& relayUrl,
        std::vector<std::string> peerNames);

    ~ListeningPeerRequestGenerator();

protected:
    virtual std::unique_ptr<nx::network::aio::BasicPollable> startRequest(
        nx::utils::MoveOnlyFunc<void()> completionHandler) override;

private:
    const nx::utils::Url m_relayUrl;
    std::vector<std::string> m_peerNames;
    nx::utils::Mutex m_mutex;

    void onCreateSessionCompletion(
        nx::cloud::relay::api::Client* client,
        nx::utils::MoveOnlyFunc<void()> completionHandler,
        cloud::relay::api::ResultCode resultCode,
        cloud::relay::api::CreateClientSessionResponse response);
};

ListeningPeerRequestGenerator::ListeningPeerRequestGenerator(
    std::size_t maxSimultaneousRequestCount,
    const nx::utils::Url& relayUrl,
    std::vector<std::string> peerNames)
    :
    base_type(maxSimultaneousRequestCount),
    m_relayUrl(relayUrl),
    m_peerNames(std::move(peerNames))
{
}

ListeningPeerRequestGenerator::~ListeningPeerRequestGenerator()
{
    pleaseStopSync();
}

std::unique_ptr<nx::network::aio::BasicPollable> ListeningPeerRequestGenerator::startRequest(
    nx::utils::MoveOnlyFunc<void()> completionHandler)
{
    QnMutexLocker lock(&m_mutex);

    auto relayClient = std::make_unique<nx::cloud::relay::api::Client>(m_relayUrl);
    relayClient->startSession(
        QnUuid::createUuid().toSimpleString().toStdString(),
        nx::utils::random::choice(m_peerNames),
        [this, relayClientPtr = relayClient.get(),
            completionHandler = std::move(completionHandler)](
                auto&&... args) mutable
        {
            onCreateSessionCompletion(
                relayClientPtr,
                std::move(completionHandler),
                std::forward<decltype(args)>(args)...);
        });

    return relayClient;
}

void ListeningPeerRequestGenerator::onCreateSessionCompletion(
    nx::cloud::relay::api::Client* client,
    nx::utils::MoveOnlyFunc<void()> completionHandler,
    cloud::relay::api::ResultCode resultCode,
    cloud::relay::api::CreateClientSessionResponse response)
{
    if (resultCode != cloud::relay::api::ResultCode::ok)
        return completionHandler();

    client->openConnectionToTheTargetHost(
        response.sessionId,
        [completionHandler = std::move(completionHandler)](
            auto&&... /*args*/)
        {
            completionHandler();
        });
}

//-------------------------------------------------------------------------------------------------

class RelayService:
    public ::testing::Test,
    public BasicComponentTest
{
public:
    RelayService():
        BasicComponentTest(Mode::singleRelay)
    {
    }

    ~RelayService()
    {
        m_requestGenerator.reset();

        stopAllInstances();

        if (m_factoryFunctionBak)
        {
            model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
                std::move(*m_factoryFunctionBak));
        }
    }

protected:
    void givenRelayThatFailedToConnectToDb()
    {
        using namespace std::placeholders;

        m_forceConnectionFailure = true;

        m_factoryFunctionBak = model::RemoteRelayPeerPoolFactory::instance().setCustomFunc(
            [this](const conf::Settings& settings)
            {
                return std::make_unique<TestRemoteRelayPeerPool>(
                    settings,
                    &m_initEvents,
                    &m_forceConnectionFailure);
            });

        addRelayInstance({"-listeningPeerDb/connectionRetryDelay", "10ms"}, false);

        ASSERT_FALSE(m_initEvents.pop());
    }

    void givenStartedRelay(std::vector<const char*> args = {})
    {
        addRelayInstance(args);
    }

    void givenManyListeningPeers()
    {
        static constexpr int kPeerCount = 71;

        std::vector<std::string> hostNames;
        for (int i = 0; i < kPeerCount; ++i)
            hostNames.push_back(nx::utils::generateRandomName(7).toStdString());

        auto peers = addListeningPeers(hostNames);
        for (std::size_t i = 0; i < peers.size(); ++i)
            m_listeningPeers.push_back({hostNames[i], std::move(peers[i])});
    }

    void whenStartDb()
    {
        m_forceConnectionFailure = false;
    }

    void thenRelayHasConnectedToDb()
    {
        while (!m_initEvents.pop()) {}
    }

    void andRelayHasStarted()
    {
        ASSERT_TRUE(relay().waitUntilStarted());
    }

    void thenRelayCanStillBeStopped()
    {
        relay().stop();
    }

    void startConnectingToRandomPeersSimultaneously()
    {
        static constexpr int kSimultaneousRequestCount = 101;

        std::vector<std::string> hostNames;
        std::transform(
            m_listeningPeers.begin(), m_listeningPeers.end(),
            std::back_inserter(hostNames),
            [](const ListeningPeer& value) { return value.hostName; });

        m_requestGenerator = std::make_unique<ListeningPeerRequestGenerator>(
            kSimultaneousRequestCount,
            relay().httpUrl(),
            std::move(hostNames));
        m_requestGenerator->start();
    }

private:
    struct ListeningPeer
    {
        std::string hostName;
        std::unique_ptr<nx::network::http::TestHttpServer> server;
    };

    bool m_forceConnectionFailure = false;
    nx::utils::SyncQueue<bool> m_initEvents;
    std::optional<model::RemoteRelayPeerPoolFactory::Function> m_factoryFunctionBak;
    std::vector<ListeningPeer> m_listeningPeers;
    std::unique_ptr<ListeningPeerRequestGenerator> m_requestGenerator;
};

// cassandra specific functionality that doesn't apply to sync engine
TEST_F(RelayService, waits_for_db_availability_if_db_host_is_specified)
{
    givenRelayThatFailedToConnectToDb();

    whenStartDb();

    thenRelayHasConnectedToDb();
    andRelayHasStarted();
}

// cassandra specific functionality that doesn't apply to sync engine
TEST_F(RelayService, can_be_stopped_regardless_of_db_host_availability)
{
    givenRelayThatFailedToConnectToDb();
    thenRelayCanStillBeStopped();
}

TEST_F(RelayService, DISABLED_can_be_stopped_under_load)
{
    givenStartedRelay();
    givenManyListeningPeers();
    startConnectingToRandomPeersSimultaneously();

    std::this_thread::sleep_for(std::chrono::seconds(nx::utils::random::number<int>(0, 7)));

    thenRelayCanStillBeStopped();
}

//-------------------------------------------------------------------------------------------------

class RelayServiceHttp:
    public RelayService
{
protected:
    void givenTcpConnectionToRelayHttpPort()
    {
        m_connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(m_connection->connect(
            nx::network::SocketAddress(
                nx::network::HostAddress::localhost,
                relay().moduleInstance()->httpEndpoints().front().port),
            nx::network::kNoTimeout)) << SystemError::getLastOSErrorText().toStdString();
    }

    void assertConnectionClosedByRelay()
    {
        char buf[1024];
        const int result = m_connection->recv(buf, sizeof(buf), 0);
        ASSERT_TRUE(result == 0 || result == -1) << "Unexpected result: " << result;
    }

private:
    std::unique_ptr<nx::network::TCPSocket> m_connection;
};

TEST_F(RelayServiceHttp, connection_inactivity_timeout_present)
{
    givenStartedRelay({"--http/connectionInactivityTimeout=1ms"});
    givenTcpConnectionToRelayHttpPort();

    assertConnectionClosedByRelay();
}

} // namespace test
} // namespace relay
} // namespace cloud
} // namespace nx
