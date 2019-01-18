#include <gtest/gtest.h>

#include <nx/cloud/db/ec2/data_conversion.h>

#include <nx/network/stream_proxy.h>
#include <nx/network/stream_server_socket_to_acceptor_wrapper.h>
#include <nx/network/system_socket.h>
#include <nx/network/test_support/squid_proxy_emulator.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include "cloud_system_fixture.h"

namespace test {

class VmsCloudDataSynchronization:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory,
    public CloudSystemFixture
{
public:
    VmsCloudDataSynchronization():
        nx::utils::test::TestWithTemporaryDirectory(
            "vms_cloud_intergration.dataSynch",
            QString()),
        CloudSystemFixture(testDataDir().toStdString())
    {
    }

    static void SetUpTestCase()
    {
        s_staticCommonModule =
            std::make_unique<QnStaticCommonModule>(nx::vms::api::PeerType::server);
    }

    static void TearDownTestCase()
    {
        s_staticCommonModule.reset();
    }

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(initializeCloud());
        registerOwnerAccount();
    }

    void registerOwnerAccount()
    {
        m_ownerAccount = cloud().registerCloudAccount();
    }

    VmsPeer& server(int index)
    {
        return m_vmsSystem->peer(index);
    }

    void waitForDataSynchronized(const VmsPeer& one, const VmsPeer& two)
    {
        while (!ec2::test::PeerWrapper::peersInterconnected(
            std::vector<const VmsPeer*>{&two, &one}))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        while (!ec2::test::PeerWrapper::allPeersHaveSameTransactionLog(
            std::vector<const VmsPeer*>{&one, &two}))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    void waitForDataSynchronized(const Cloud& /*cloud*/, const VmsPeer& peer)
    {
        waitUntilVmsTransactionLogMatchesCloudOne(
            peer,
            m_vmsSystem->cloudSystemId(),
            m_ownerAccount);
    }

    void startServer(int index)
    {
        ASSERT_TRUE(m_vmsSystem->peer(index).startAndWaitUntilStarted());
    }

    void stopServer(int index)
    {
        m_vmsSystem->peer(index).stop();
    }

    void givenCloudSystemWithServerCount(int count)
    {
        m_vmsSystem = createVmsSystem(count);
        ASSERT_NE(nullptr, m_vmsSystem);

        ASSERT_TRUE(m_vmsSystem->connectToCloud(&cloud(), m_ownerAccount));
    }

    void addRandomNonCloudDataToServer(int index)
    {
        static const QnUuid kUserResourceTypeGuid("{774e6ecd-ffc6-ae88-0165-8f4a6d0eafa7}");

        nx::vms::api::UserData userData;
        userData.id = QnUuid::createUuid();
        userData.typeId = kUserResourceTypeGuid;
        userData.name = "local user " + nx::utils::generateRandomName(7);
        userData.isEnabled = true;
        userData.realm = "VMS";

        auto client = m_vmsSystem->peer(index).mediaServerClient();
        ASSERT_EQ(::ec2::ErrorCode::ok, client->ec2SaveUser(userData));
    }

    void addCloudUserOnServer(int index)
    {
        m_cloudAccounts.push_back(cloud().registerCloudAccount());

        auto client = m_vmsSystem->peer(index).mediaServerClient();

        nx::cloud::db::api::SystemSharing systemSharing;
        systemSharing.accessRole = nx::cloud::db::api::SystemAccessRole::advancedViewer;
        systemSharing.isEnabled = true;
        systemSharing.accountEmail = m_cloudAccounts.back().email;
        systemSharing.systemId = m_vmsSystem->cloudSystemId();
        systemSharing.vmsUserId =
            QnUuid::fromArbitraryData(m_cloudAccounts.back().email).toSimpleString().toStdString();

        nx::vms::api::UserData userData;
        nx::cloud::db::ec2::convert(systemSharing, &userData);

        ASSERT_EQ(::ec2::ErrorCode::ok, client->ec2SaveUser(userData));
    }

    void addCloudUserOnCloud()
    {
        const auto account = cloud().registerCloudAccount();
        ASSERT_EQ(
            nx::cloud::db::api::ResultCode::ok,
            cloud().cdb().shareSystem(
                m_ownerAccount,
                m_vmsSystem->cloudSystemId(),
                account.email,
                nx::cloud::db::api::SystemAccessRole::viewer));
    }

private:
    std::unique_ptr<VmsSystem> m_vmsSystem;
    nx::cloud::db::AccountWithPassword m_ownerAccount;
    std::vector<nx::cloud::db::AccountWithPassword> m_cloudAccounts;

    static std::unique_ptr<QnStaticCommonModule> s_staticCommonModule;
};

std::unique_ptr<QnStaticCommonModule> VmsCloudDataSynchronization::s_staticCommonModule;

TEST_F(VmsCloudDataSynchronization, data_is_synchronized)
{
    givenCloudSystemWithServerCount(1);

    addCloudUserOnServer(0);
    addCloudUserOnCloud();

    waitForDataSynchronized(cloud(), server(0));
}

TEST_F(
    VmsCloudDataSynchronization,
    data_added_while_mediaserver_is_offline_is_synchronized_when_back_online)
{
    givenCloudSystemWithServerCount(1);

    addCloudUserOnServer(0);
    addCloudUserOnCloud();
    waitForDataSynchronized(cloud(), server(0));

    stopServer(0);
    addCloudUserOnCloud();
    startServer(0);

    waitForDataSynchronized(cloud(), server(0));
}

TEST_F(
    VmsCloudDataSynchronization,
    another_mediaserver_synchronizes_data_to_cloud)
{
    givenCloudSystemWithServerCount(2);
    stopServer(1);

    addRandomNonCloudDataToServer(0);
    addCloudUserOnServer(0);
    waitForDataSynchronized(cloud(), server(0));
}

TEST_F(VmsCloudDataSynchronization, using_cloud_does_not_trim_data)
{
    givenCloudSystemWithServerCount(2);
    stopServer(1);

    addRandomNonCloudDataToServer(0);
    addCloudUserOnServer(0);
    waitForDataSynchronized(cloud(), server(0));
    stopServer(0);

    startServer(1);
    waitForDataSynchronized(cloud(), server(1));
    addRandomNonCloudDataToServer(1);

    startServer(0);

    // For some reasons peers won't connect without a butt kick.
    server(0).process().moduleInstance()->connectTo(
        server(1).process().moduleInstance().get());

    waitForDataSynchronized(server(0), server(1));
}

//-------------------------------------------------------------------------------------------------

class VmsCloudDataSynchronizationThroughFirewall:
    public VmsCloudDataSynchronization
{
    using base_type = VmsCloudDataSynchronization;

protected:
    virtual void SetUp() override
    {
        ASSERT_TRUE(initializeCloud());

        auto tcpServerSocket = std::make_unique<nx::network::TCPServerSocket>(AF_INET);
        ASSERT_TRUE(tcpServerSocket->bind(nx::network::SocketAddress::anyPrivateAddress));
        ASSERT_TRUE(tcpServerSocket->listen());
        ASSERT_TRUE(tcpServerSocket->setNonBlockingMode(true));

        const auto serverEndpoint = tcpServerSocket->getLocalAddress();

        auto proxy = std::make_unique<nx::network::test::SquidProxyEmulator>();
        proxy->startProxy(
            std::make_unique<nx::network::StreamServerSocketToAcceptorWrapper>(
                std::move(tcpServerSocket)),
            cloud().cdbEndpoint());

        cloud().installProxyBeforeCdb(std::move(proxy), serverEndpoint);

        registerOwnerAccount();
    }
};

TEST_F(VmsCloudDataSynchronizationThroughFirewall, DISABLED_data_is_synchronized)
{
    givenCloudSystemWithServerCount(1);

    addCloudUserOnServer(0);
    addCloudUserOnCloud();

    waitForDataSynchronized(cloud(), server(0));
}

} // namespace tests
