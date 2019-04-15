#pragma once

#include <gtest/gtest.h>

#include <nx/network/cloud/mediator/api/mediator_api_client.h>
#include <nx/network/cloud/mediator/api/mediator_api_http_paths.h>
#include <nx/cloud/discovery/test_support/discovery_server.h>

#include "mediator_cluster.h"

namespace nx::hpm::test {

class MediatorScalabilityTestFixture:
    public testing::Test,
    public api::AbstractCloudSystemCredentialsProvider
{
protected:
    static constexpr char kClusterId[] = "mediator_test_cluster";
    static constexpr int kMaxMediators = 2;

public:
    ~MediatorScalabilityTestFixture();

    virtual std::optional<nx::hpm::api::SystemCredentials> getSystemCredentials() const override;

protected:
    virtual void SetUp() override;

    void addMediator();

    void givenMultipleMediators();

    void givenSynchronizedClusterWithListeningServer();

    void whenAddServer();

    void whenMediaServerGoesOffline();

    void thenServerInfoIsSynchronized();

    void thenServerInfoIsDroppedFromCluster();

    void thenRequestIsRedirected();

protected:
    nx::cloud::discovery::test::DiscoveryServer m_discoveryServer;
    MediatorCluster m_mediatorCluster;

    AbstractCloudDataProvider::System m_system;
    std::unique_ptr<MediaServerEmulator> m_mediaServer;
    std::string m_mediaServerFullName;

    std::vector<std::shared_ptr<nx::network::stun::AsyncClient>> m_serverConnections;
    nx::utils::SyncQueue<nx::network::stun::AsyncClient*> m_closeConnectionEvents;
    nx::utils::SyncQueue<std::tuple<nx::hpm::api::ResultCode, nx::hpm::api::ListenResponse>>
        m_listenResponseQueue;

    nx::utils::SyncQueue<std::tuple<api::ResultCode, api::ConnectResponse>> m_connectResponseQueue;
};


} // namespace nx::hpm::test