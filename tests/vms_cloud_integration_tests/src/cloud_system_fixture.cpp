#include "cloud_system_fixture.h"

#include <optional>

#include <gtest/gtest.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>

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

void Cloud::installProxyBeforeCdb(
    std::unique_ptr<nx::network::StreamProxy> proxy,
    const nx::network::SocketAddress& proxyEndpoint)
{
    m_cdbProxy = std::move(proxy);
    m_proxyEndpoint = proxyEndpoint;
}

nx::cloud::db::AccountWithPassword Cloud::registerCloudAccount()
{
    return m_cdb.addActivatedAccount2();
}

bool Cloud::connectToCloud(
    VmsPeer& peerWrapper,
    const nx::cloud::db::AccountWithPassword& ownerAccount)
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

nx::cloud::db::CdbLauncher& Cloud::cdb()
{
    return m_cdb;
}

const nx::cloud::db::CdbLauncher& Cloud::cdb() const
{
    return m_cdb;
}

nx::network::SocketAddress Cloud::cdbEndpoint() const
{
    return m_proxyEndpoint ? *m_proxyEndpoint : m_cdb.endpoint();
}

bool Cloud::isSystemRegistered(
    const nx::cloud::db::AccountWithPassword& account,
    const std::string& cloudSystemId) const
{
    nx::cloud::db::api::SystemDataEx system;
    const auto resultCode =
        m_cdb.getSystem(account.email, account.password, cloudSystemId, &system);
    return resultCode == nx::cloud::db::api::ResultCode::ok;
}

bool Cloud::isSystemOnline(
    const nx::cloud::db::AccountWithPassword& account,
    const std::string& cloudSystemId) const
{
    nx::cloud::db::api::SystemDataEx system;
    if (m_cdb.getSystem(account.email, account.password, cloudSystemId, &system) !=
            nx::cloud::db::api::ResultCode::ok)
    {
        return false;
    }

    return system.stateOfHealth == nx::cloud::db::api::SystemHealth::online;
}

//-------------------------------------------------------------------------------------------------

VmsSystem::VmsSystem(std::vector<std::unique_ptr<VmsPeer>> servers):
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
        if (VmsPeer::peersInterconnected(m_servers))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void VmsSystem::waitUntilAllServersSynchronizedData()
{
    for (;;)
    {
        if (VmsPeer::allPeersHaveSameTransactionLog(m_servers))
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

const VmsPeer& VmsSystem::peer(int index) const
{
    return *m_servers[index];
}

VmsPeer& VmsSystem::peer(int index)
{
    return *m_servers[index];
}

std::unique_ptr<VmsSystem> VmsSystem::detachServer(int index)
{
    if (!m_servers[index]->detachFromSystem())
        return nullptr;

    auto detachedServer = std::move(m_servers[index]);
    m_servers.erase(m_servers.begin() + index);

    std::vector<std::unique_ptr<VmsPeer>> newSystemServers;
    newSystemServers.push_back(std::move(detachedServer));

    return std::make_unique<VmsSystem>(std::move(newSystemServers));
}

bool VmsSystem::connectToCloud(
    Cloud* cloud,
    const nx::cloud::db::AccountWithPassword& cloudAccount)
{
    return cloud->connectToCloud(peer(0), cloudAccount);
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

CloudSystemFixture::CloudSystemFixture(const std::string& baseDir):
    m_baseDir(baseDir)
{
}

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
    ::ec2::test::SystemMergeFixture systemMergeFixture(m_baseDir + "/vms_peers/");

    systemMergeFixture.addSetting(
        "-cloudIntegration/cloudDbUrl",
        nx::network::url::Builder().setScheme(nx::network::http::kUrlSchemeName)
            .setEndpoint(m_cloud.cdbEndpoint()).toString().toStdString());

    systemMergeFixture.addSetting(
        "-cloudIntegration/delayBeforeSettingMasterFlag",
        "100ms");

    if (!systemMergeFixture.initializeSingleServerSystems(serverCount))
        return nullptr;

    if (serverCount > 1)
    {
        if (systemMergeFixture.mergeSystems() != QnRestResult::NoError)
            return nullptr;
        systemMergeFixture.waitUntilAllServersSynchronizedData();
    }

    return std::make_unique<VmsSystem>(systemMergeFixture.takeServers());
}

void CloudSystemFixture::waitUntilVmsTransactionLogMatchesCloudOne(
    const VmsPeer& vmsPeer,
    const std::string& cloudSystemId,
    const nx::cloud::db::AccountWithPassword& account)
{
    auto mediaServerClient = vmsPeer.mediaServerClient();

    for (;;)
    {
        ::ec2::ApiTranLogFilter filter;
        filter.cloudOnly = true;
        ::ec2::ApiTransactionDataList vmsTransactionLog;
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            mediaServerClient->ec2GetTransactionLog(filter, &vmsTransactionLog));

        ::ec2::ApiTransactionDataList cloudTransactionLog;
        m_cloud.cdb().getTransactionLog(
            account.email,
            account.password,
            cloudSystemId,
            &cloudTransactionLog);

        if (vmsTransactionLog == cloudTransactionLog)
            break;

        NX_VERBOSE(this, "VMS transaction log: %1", QJson::serialized(vmsTransactionLog));
        NX_VERBOSE(this, "Cloud transaction log: %1", QJson::serialized(cloudTransactionLog));

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void CloudSystemFixture::waitUntilVmsTransactionLogMatchesCloudOne(
    VmsSystem* vmsSystem,
    const nx::cloud::db::AccountWithPassword& account)
{
    waitUntilVmsTransactionLogMatchesCloudOne(
        vmsSystem->peer(0),
        vmsSystem->cloudSystemId(),
        account);
}

} // namespace test
