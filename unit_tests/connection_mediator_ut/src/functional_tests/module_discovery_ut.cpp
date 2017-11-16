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
#include <discovery/discovery_http_api_path.h>

#include "../integration_tests/mediator_relay_integration_test_setup.h"

namespace nx {
namespace hpm {
namespace test {

namespace {

struct InstanceInformation:
    nx::cloud::discovery::BasicInstanceInformation
{
    static const char* const kTypeName;

    InstanceInformation():
        nx::cloud::discovery::BasicInstanceInformation(kTypeName)
    {
    }
};

const char* const InstanceInformation::kTypeName = "TestModule";

using DiscoveryClient = nx::cloud::discovery::ModuleRegistrar<InstanceInformation>;

} // namespace

class DiscoveryHttpApi:
    public MediatorRelayIntegrationTestSetup
{
    using base_type = MediatorRelayIntegrationTestSetup;

public:
    DiscoveryHttpApi():
        m_moduleId(QnUuid::createUuid().toSimpleString().toStdString()),
        m_moduleUrl("http://nxvms.com/relay_api")
    {
    }

    ~DiscoveryHttpApi()
    {
        if (m_moduleRegistrar)
            m_moduleRegistrar->pleaseStopSync();
    }

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        auto discoveryUrl = nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
            .setEndpoint(httpEndpoint()).toUrl();
        m_moduleRegistrar = std::make_unique<DiscoveryClient>(discoveryUrl, m_moduleId);

        InstanceInformation info;
        info.id = m_moduleId;
        info.apiUrl = m_moduleUrl;
        m_moduleRegistrar->setInstanceInformation(std::move(info));
    }

    void givenRegisteredModule()
    {
        whenModulesRegistersItSelf();
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

    void whenModulesRegistersItSelf()
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
        ASSERT_EQ(m_moduleUrl, nx::utils::Url(*relayUrl));
    }

    void thenNoModuleUrlHasBeenReported()
    {
        assertTrafficRelayUrlHasBeenReported();
    }

private:
    std::string m_moduleId;
    nx::utils::Url m_moduleUrl;
    std::unique_ptr<DiscoveryClient> m_moduleRegistrar;

    nx::cloud::discovery::PeerStatus fetchPeerStatus()
    {
        nx::cloud::discovery::ModuleFinder moduleFinder(
            nx::network::url::Builder().setScheme(nx_http::kUrlSchemeName)
                .setEndpoint(httpEndpoint()).toUrl());
        nx::utils::promise<
            std::tuple<nx::cloud::discovery::ResultCode, std::vector<InstanceInformation>>
        > done;
        moduleFinder.fetchModules<InstanceInformation>(
            [&done](
                nx::cloud::discovery::ResultCode resultCode,
                std::vector<InstanceInformation> instanceInformation)
            {
                done.set_value(std::make_tuple(resultCode, std::move(instanceInformation)));
            });
        const auto result = done.get_future().get();
        const auto resultCode = std::get<0>(result);
        if (resultCode != nx::cloud::discovery::ResultCode::ok)
        {
            throw std::runtime_error(
                "Error fetching peer status. " +
                QnLexical::serialized(resultCode).toStdString());
        }

        const auto instanceInformationVector = std::get<1>(result);
        for (const auto& instanceInformation: instanceInformationVector)
        {
            if (instanceInformation.id == m_moduleId)
                return nx::cloud::discovery::PeerStatus::online;
        }

        return nx::cloud::discovery::PeerStatus::offline;
    }
};

TEST_F(DiscoveryHttpApi, peer_is_considered_alive_just_after_registration)
{
    whenModulesRegistersItSelf();
    thenModuleIsConsideredOnline();
}

TEST_F(DiscoveryHttpApi, peer_removed_after_expiration_timeout)
{
    givenRegisteredModule();
    whenModuleStops();
    thenModuleIsConsideredOffline();
}

TEST_F(DiscoveryHttpApi, no_relay_url_is_reported_if_no_relay_registered)
{
    whenInvokeStunListen();
    thenNoModuleUrlHasBeenReported();
}

TEST_F(DiscoveryHttpApi, DISABLED_online_relay_is_selected)
{
    givenRegisteredModule();
    whenInvokeStunListen();
    thenOnlineModuleUrlHasBeenReported();
}

TEST_F(DiscoveryHttpApi, offline_relay_is_no_longer_selected)
{
    givenRegisteredModule();
    whenModuleGoesOffline();
    whenInvokeStunListen();
    thenNoModuleUrlHasBeenReported();
}

} // namespace test
} // namespace hpm
} // namespace nx
