#include <memory>
#include <string>

#include <gtest/gtest.h>

#include <QtCore/QUrl>

#include <nx/network/aio/basic_pollable.h>
#include <nx/network/http/http_client.h>
#include <nx/network/http/fusion_data_http_client.h>
#include <nx/network/http/rest/http_rest_client.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/std/cpp14.h>

#include <discovery/discovery_client.h>
#include <http/http_api_path.h>

#include "../integration_tests/mediator_relay_integration_test_setup.h"

namespace nx {
namespace hpm {
namespace test {

namespace {

struct InstanceInformation:
    nx::cloud::discovery::BasicInstanceInformation
{
    static const char* const kTypeName;
};

const char* const InstanceInformation::kTypeName = "TestModule";

using DiscoveryClient = nx::cloud::discovery::ModuleRegistrar<InstanceInformation>;

} // namespace

class ModuleDiscovery:
    public MediatorRelayIntegrationTestSetup
{
    using base_type = MediatorRelayIntegrationTestSetup;

public:
    ModuleDiscovery():
        m_relayId(QnUuid::createUuid().toSimpleString().toStdString()),
        m_relayUrl("http://nxvms.com/relay_api")
    {
    }

    ~ModuleDiscovery()
    {
        if (m_moduleRegistrar)
            m_moduleRegistrar->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_moduleRegistrar = std::make_unique<DiscoveryClient>(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(httpEndpoint()).toUrl());

        InstanceInformation info;
        info.id = m_relayId;
        info.apiUrl = m_relayUrl;
        m_moduleRegistrar->setInstanceInformation(std::move(info));
    }

    void givenRegisteredModule()
    {
        whenModuleRelay();
        thenModuleIsConsideredOnline();
    }

    void whenModuleStops()
    {
        m_moduleRegistrar->pleaseStopSync();
    }

    void thenModuleIsConsideredOnline()
    {
        while (fetchPeerStatus() != nx::cloud::discovery::PeerStatus::online)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void thenModuleIsConsideredOffline()
    {
        while (fetchPeerStatus() != nx::cloud::discovery::PeerStatus::offline)
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    void whenModuleRelay()
    {
        m_moduleRegistrar->start();
    }

    void whenInvokeStunListen()
    {
        issueListenRequest();
    }

    void whenModuleGoesOffline()
    {
        whenModuleStops();
        thenModuleIsConsideredOffline();
    }

    void thenOnlineModuleUrlHasBeenReported()
    {
        const auto relayUrl = reportedTrafficRelayUrl();
        ASSERT_TRUE(static_cast<bool>(relayUrl));
        ASSERT_EQ(m_relayUrl, QUrl(*relayUrl));
    }

    void thenNoModuleUrlHasBeenReported()
    {
        assertTrafficRelayUrlHasBeenReported();
    }

private:
    std::string m_relayId;
    QUrl m_relayUrl;
    std::unique_ptr<DiscoveryClient> m_moduleRegistrar;

    nx::cloud::discovery::PeerStatus fetchPeerStatus()
    {
        const auto url = nx::network::url::Builder()
            .setScheme(nx_http::kUrlSchemeName).setEndpoint(httpEndpoint())
            .setPath(nx_http::rest::substituteParameters(
                http::kDiscoveryModulePath, {m_relayId.c_str()})
            ).toUrl();

        nx_http::FusionDataHttpClient<void, InstanceInformation> httpClient(
            url, nx_http::AuthInfo());

        nx::utils::promise<boost::optional<InstanceInformation>> done;
        httpClient.execute(
            [&done](
                SystemError::ErrorCode systemErrorCode,
                const nx_http::Response* response,
                InstanceInformation instanceInformation)
            {
                if (response && nx_http::StatusCode::isSuccessCode(response->statusLine.statusCode))
                    done.set_value(std::move(instanceInformation));
                else
                    done.set_value(boost::none);
            });
        const auto result = done.get_future().get();
        if (!result)
            throw std::runtime_error("Error fetching peer status");

        return result->status;
    }
};

#if 0

TEST_F(ModuleDiscovery, peer_is_considered_alive_just_after_registration)
{
    whenModuleRelay();
    thenModuleIsConsideredOnline();
}

TEST_F(ModuleDiscovery, peer_removed_after_expiration_timeout)
{
    givenRegisteredModule();
    whenModuleStops();
    thenModuleIsConsideredOffline();
}

TEST_F(ModuleDiscovery, no_relay_url_is_reported_if_no_relay_registered)
{
    whenInvokeStunListen();
    thenNoModuleUrlHasBeenReported();
}

TEST_F(ModuleDiscovery, online_relay_is_selected)
{
    givenRegisteredModule();
    whenInvokeStunListen();
    thenOnlineModuleUrlHasBeenReported();
}

TEST_F(ModuleDiscovery, offline_relay_is_no_longer_selected)
{
    givenRegisteredModule();
    whenModuleGoesOffline();
    whenInvokeStunListen();
    thenNoModuleUrlHasBeenReported();
}

#endif

} // namespace test
} // namespace hpm
} // namespace nx
