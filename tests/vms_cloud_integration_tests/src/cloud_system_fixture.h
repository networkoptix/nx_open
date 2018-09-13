#pragma once

#include <memory>

#include <nx/cloud/cdb/test_support/cdb_launcher.h>

#include <nx/network/http/test_http_server.h>

#include <test_support/merge_test_fixture.h>

namespace test {

class VmsSystem;

class Cloud
{
public:
    ~Cloud();

    bool initialize();

    nx::cdb::AccountWithPassword registerCloudAccount();

    bool connectToCloud(
        ::ec2::test::PeerWrapper& peerWrapper,
        const nx::cdb::AccountWithPassword& ownerAccount);

    nx::cdb::CdbLauncher& cdb();
    const nx::cdb::CdbLauncher& cdb() const;

    bool isSystemRegistered(
        const nx::cdb::AccountWithPassword& account,
        const std::string& cloudSystemId) const;

    bool isSystemOnline(
        const nx::cdb::AccountWithPassword& account,
        const std::string& cloudSystemId) const;

private:
    mutable nx::cdb::CdbLauncher m_cdb;
    nx::network::http::TestHttpServer m_httpProxy;
    std::vector<std::string> m_customHostNames;
};

//-------------------------------------------------------------------------------------------------

class VmsSystem
{
public:
    VmsSystem(std::vector<std::unique_ptr<::ec2::test::PeerWrapper>> servers);

    int peerCount() const;

    void waitUntilAllServersAreInterconnected();
    void waitUntilAllServersSynchronizedData();

    const ::ec2::test::PeerWrapper& peer(int index) const;
    ::ec2::test::PeerWrapper& peer(int index);

    std::unique_ptr<VmsSystem> detachServer(int index);

    bool connectToCloud(
        Cloud* cloud,
        const nx::cdb::AccountWithPassword& cloudAccount);

    bool isConnectedToCloud() const;

    std::string cloudSystemId() const;

private:
    std::vector<std::unique_ptr<::ec2::test::PeerWrapper>> m_servers;
};

//-------------------------------------------------------------------------------------------------

class CloudSystemFixture
{
public:
    bool initializeCloud();

    Cloud& cloud();

    std::unique_ptr<VmsSystem> createVmsSystem(int serverCount);

    void waitUntilVmsTransactionLogMatchesCloudOne(
        VmsSystem* vmsSystem,
        const nx::cdb::AccountWithPassword& account);

private:
    Cloud m_cloud;
};

} // namespace test
