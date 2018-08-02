#include "cloud_system_fixture.h"

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>

namespace test {

Cloud::~Cloud()
{
    for (const auto& systemId: m_customHostNames)
        nx::network::SocketGlobals::addressResolver().removeFixedAddress(systemId);
}

bool Cloud::initialize()
{
    if (!m_httpProxy.bindAndListen())
        return false;

    m_cdb.addArg(
        "-vmsGateway/url",
        lm("http://%1/{targetSystemId}")
            .arg(m_httpProxy.serverAddress()).toStdString().c_str());

    return m_cdb.startAndWaitUntilStarted();
}

nx::cdb::AccountWithPassword Cloud::registerCloudAccount()
{
    return m_cdb.addActivatedAccount2();
}

bool Cloud::connectToCloud(
    ::ec2::test::PeerWrapper& peerWrapper,
    const nx::cdb::AccountWithPassword& ownerAccount)
{
    const auto system = m_cdb.addRandomSystemToAccount(ownerAccount);
    if (!peerWrapper.saveCloudSystemCredentials(
            system.id,
            system.authKey,
            ownerAccount.email))
    {
        return false;
    }

    if (!m_httpProxy.registerRedirectHandler(
            lm("/%1/api/mergeSystems").args(system.id),
            nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
                .setEndpoint(peerWrapper.endpoint()).setPath("/api/mergeSystems")))
    {
        return false;
    }

    nx::network::SocketGlobals::addressResolver().addFixedAddress(
        system.id,
        peerWrapper.endpoint());
    m_customHostNames.push_back(system.id);
    return true;
}

nx::cdb::CdbLauncher& Cloud::cdb()
{
    return m_cdb;
}

const nx::cdb::CdbLauncher& Cloud::cdb() const
{
    return m_cdb;
}

bool Cloud::isSystemRegistered(
    const nx::cdb::AccountWithPassword& account,
    const std::string& cloudSystemId) const
{
#if 1
    std::vector<nx::cdb::api::SystemDataEx> systems;
    const auto resultCode = m_cdb.getSystems(
        account.email, account.password,
        &systems);
    if (resultCode != nx::cdb::api::ResultCode::ok)
        return false;
    const auto isFound = std::any_of(
        systems.begin(), systems.end(),
        [cloudSystemId](auto system) { return system.id == cloudSystemId; });
    if (!isFound)
        int x = 0;
    return isFound;
#else
    nx::cdb::api::SystemDataEx system;
    const auto resultCode =
        m_cdb.getSystem(account.email, account.password, cloudSystemId, &system);
    if (resultCode != nx::cdb::api::ResultCode::ok)
        return false;
#endif

    return true;
}

bool Cloud::isSystemOnline(
    const nx::cdb::AccountWithPassword& account,
    const std::string& cloudSystemId) const
{
    nx::cdb::api::SystemDataEx system;
    if (m_cdb.getSystem(account.email, account.password, cloudSystemId, &system) !=
            nx::cdb::api::ResultCode::ok)
    {
        return false;
    }

    return system.stateOfHealth == nx::cdb::api::SystemHealth::online;
}

//-------------------------------------------------------------------------------------------------

VmsSystem::VmsSystem(std::vector<std::unique_ptr<::ec2::test::PeerWrapper>> servers):
    m_servers(std::move(servers))
{
}

int VmsSystem::peerCount() const
{
    return (int)m_servers.size();
}

void VmsSystem::waitUntilAllServersAreInterconnected()
{
    for (;;)
    {
        if (::ec2::test::PeerWrapper::arePeersInterconnected(m_servers))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void VmsSystem::waitUntilAllServersSynchronizedData()
{
    for (;;)
    {
        if (::ec2::test::PeerWrapper::areAllPeersHaveSameTransactionLog(m_servers))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

const ::ec2::test::PeerWrapper& VmsSystem::peer(int index) const
{
    return *m_servers[index];
}

::ec2::test::PeerWrapper& VmsSystem::peer(int index)
{
    return *m_servers[index];
}

std::unique_ptr<VmsSystem> VmsSystem::detachServer(int index)
{
    if (!m_servers[index]->detachFromSystem())
        return nullptr;

    auto detachedServer = std::move(m_servers[index]);
    m_servers.erase(m_servers.begin() + index);

    std::vector<std::unique_ptr<::ec2::test::PeerWrapper>> newSystemServers;
    newSystemServers.push_back(std::move(detachedServer));

    return std::make_unique<VmsSystem>(std::move(newSystemServers));
}

bool VmsSystem::connectToCloud(
    Cloud* cloud,
    const nx::cdb::AccountWithPassword& cloudAccount)
{
    if (!cloud->connectToCloud(peer(0), cloudAccount))
        return false;

    // TODO

    return true;
}

bool VmsSystem::isConnectedToCloud() const
{
    return !cloudSystemId().empty();
}

std::string VmsSystem::cloudSystemId() const
{
    return m_servers.front()->getCloudCredentials().systemId.toStdString();
}

//-------------------------------------------------------------------------------------------------

bool CloudSystemFixture::initializeCloud()
{
    return m_cloud.initialize();
}

Cloud& CloudSystemFixture::cloud()
{
    return m_cloud;
}

std::unique_ptr<VmsSystem> CloudSystemFixture::createVmsSystem(int serverCount)
{
    ::ec2::test::SystemMergeFixture systemMergeFixture;

    systemMergeFixture.addSetting(
        "-cloudIntegration/cloudDbUrl",
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_cloud.cdb().endpoint()).toString().toStdString());

    systemMergeFixture.addSetting(
        "-cloudIntegration/delayBeforeSettingMasterFlag",
        "100ms");

    if (!systemMergeFixture.initializeSingleServerSystems(serverCount))
        return false;

    if (serverCount > 1)
    {
        systemMergeFixture.mergeSystems();
        systemMergeFixture.waitUntilAllServersSynchronizedData();
    }

    return std::make_unique<VmsSystem>(systemMergeFixture.takeServers());
}

void CloudSystemFixture::waitUntilVmsTransactionLogMatchesCloudOne(
    VmsSystem* vmsSystem,
    const nx::cdb::AccountWithPassword& account)
{
    auto mediaServerClient = vmsSystem->peer(0).mediaServerClient();

    for (;;)
    {
        ::ec2::ApiTranLogFilter filter;
        filter.cloudOnly = true;
        ::ec2::ApiTransactionDataList vmsTransactionLog;
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            mediaServerClient->ec2GetTransactionLog(filter, &vmsTransactionLog));

        ::ec2::ApiTransactionDataList fullVmsTransactionLog;
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            mediaServerClient->ec2GetTransactionLog(
                ::ec2::ApiTranLogFilter(), &fullVmsTransactionLog));

        ::ec2::ApiTransactionDataList cloudTransactionLog;
        m_cloud.cdb().getTransactionLog(
            account.email,
            account.password,
            vmsSystem->cloudSystemId(),
            &cloudTransactionLog);

        if (vmsTransactionLog == cloudTransactionLog)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

} // namespace test
