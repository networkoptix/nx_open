#pragma once

#include <memory>

#include <nx/cloud/db/test_support/cdb_launcher.h>

#include <nx/network/http/test_http_server.h>
#include <nx/network/stream_proxy.h>

#include <test_support/merge_test_fixture.h>

namespace test {

class VmsSystem;
using VmsPeer = ::ec2::test::PeerWrapper;

class Cloud
{
public:
    ~Cloud();

    bool initialize();

    void installProxyBeforeCdb(
        std::unique_ptr<nx::network::StreamProxy> proxy,
        const nx::network::SocketAddress& proxyEndpoint);

    nx::cloud::db::AccountWithPassword registerCloudAccount();

    bool connectToCloud(
        VmsPeer& peerWrapper,
        const nx::cloud::db::AccountWithPassword& ownerAccount);

    nx::cloud::db::CdbLauncher& cdb();
    const nx::cloud::db::CdbLauncher& cdb() const;

    nx::network::SocketAddress cdbEndpoint() const;

    bool isSystemRegistered(
        const nx::cloud::db::AccountWithPassword& account,
        const std::string& cloudSystemId) const;

    bool isSystemOnline(
        const nx::cloud::db::AccountWithPassword& account,
        const std::string& cloudSystemId) const;

private:
    mutable nx::cloud::db::CdbLauncher m_cdb;
    std::unique_ptr<nx::network::StreamProxy> m_cdbProxy;
    nx::network::http::TestHttpServer m_httpProxy;
    std::vector<std::string> m_customHostNames;
    std::optional<nx::network::SocketAddress> m_proxyEndpoint;
};

//-------------------------------------------------------------------------------------------------

class VmsSystem
{
public:
    VmsSystem(std::vector<std::unique_ptr<VmsPeer>> servers);

    int peerCount() const;

    void waitUntilAllServersAreInterconnected();
    void waitUntilAllServersSynchronizedData();

    const VmsPeer& peer(int index) const;
    VmsPeer& peer(int index);

    std::unique_ptr<VmsSystem> detachServer(int index);

    bool connectToCloud(
        Cloud* cloud,
        const nx::cloud::db::AccountWithPassword& cloudAccount);

    bool isConnectedToCloud() const;

    std::string cloudSystemId() const;

private:
    std::vector<std::unique_ptr<VmsPeer>> m_servers;
};

//-------------------------------------------------------------------------------------------------

class CloudSystemFixture
{
public:
    CloudSystemFixture(const std::string& baseDir);
    virtual ~CloudSystemFixture() = default;

    bool initializeCloud();

    Cloud& cloud();

    std::unique_ptr<VmsSystem> createVmsSystem(int serverCount);

    void waitUntilVmsTransactionLogMatchesCloudOne(
        const VmsPeer& vmsPeer,
        const std::string& cloudSystemId,
        const nx::cloud::db::AccountWithPassword& account);

    void waitUntilVmsTransactionLogMatchesCloudOne(
        VmsSystem* vmsSystem,
        const nx::cloud::db::AccountWithPassword& account);

private:
    std::string m_baseDir;
    Cloud m_cloud;
};

} // namespace test
