#pragma once

#include <memory>

#include <relay/relay_cluster_client_factory.h>
#include <test_support/mediaserver_emulator.h>

#include "../functional_tests/mediator_functional_test.h"

namespace nx {
namespace hpm {
namespace test {

class MediatorRelayIntegrationTestSetup:
    public MediatorFunctionalTest,
    public api::AbstractCloudSystemCredentialsProvider
{
public:
    MediatorRelayIntegrationTestSetup();
    ~MediatorRelayIntegrationTestSetup();

protected:
    virtual void SetUp() override;

    void issueListenRequest();
    void givenListeningServer();
    void issueConnectRequest();
    void assertTrafficRelayUrlHasBeenReported();
    boost::optional<nx::String> reportedTrafficRelayUrl();

private:
    const QUrl m_relayUrl;
    api::ResultCode m_lastRequestResult = api::ResultCode::ok;
    boost::optional<api::ListenResponse> m_listenResponse;
    boost::optional<api::ConnectResponse> m_connectResponse;
    std::shared_ptr<nx::network::stun::AbstractAsyncClient> m_stunClient;
    std::unique_ptr<api::MediatorServerTcpConnection> m_serverConnection;
    std::unique_ptr<api::MediatorClientUdpConnection> m_mediatorUdpClient;
    api::SystemCredentials m_systemCredentials;
    std::unique_ptr<MediaServerEmulator> m_mediaServerEmulator;
    RelayClusterClientFactory::Function m_factoryFuncBak;

    virtual boost::optional<api::SystemCredentials> getSystemCredentials() const override;

    std::unique_ptr<AbstractRelayClusterClient> createRelayClusterClient(
        const conf::Settings& /*settings*/);
};

} // namespace test
} // namespace hpm
} // namespace nx
