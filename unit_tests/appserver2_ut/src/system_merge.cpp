#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/std/cpp14.h>

#include <api/mediaserver_client.h>
#include <test_support/appserver2_process.h>
#include <test_support/peer_wrapper.h>
#include <test_support/merge_test_fixture.h>
#include <transaction/transaction.h>

namespace ec2 {
namespace test {

class SystemMerge:
    public ::testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory,
    public SystemMergeFixture
{
public:
    SystemMerge():
        nx::utils::test::TestWithTemporaryDirectory("appserver2_ut.SystemMerge", QString()),
        SystemMergeFixture(testDataDir().toStdString())
    {
    }

protected:
    void givenTwoSingleServerSystems()
    {
        ASSERT_TRUE(initializeSingleServerSystems(2));
    }

    void givenTwoSystemsWithServersWithSameId()
    {
        givenTwoSingleServerSystems();

        nx::vms::api::MediaServerData newServer;
        newServer.id = QnUuid::createUuid();
        newServer.name = "Shared server";

        for (int i = 0; i < peerCount(); ++i)
        {
            ASSERT_EQ(
                ec2::ErrorCode::ok,
                peer(i).mediaServerClient()->ec2SaveMediaServer(newServer));
        }
    }

    void merge(
        PeerWrapper& what,
        PeerWrapper& to,
        std::vector<MergeAttributes::Attribute> params)
    {
        ASSERT_EQ(QnRestResult::NoError, mergePeers(what, to, std::move(params)));
    }

    void thenMergeSucceeded()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, prevMergeResult());
    }

    void thenMergedFailed()
    {
        ASSERT_NE(QnRestResult::Error::NoError, prevMergeResult());
    }

    void thenMergeHistoryRecordIsAdded()
    {
        waitUntilMergeHistoryIsAdded();
    }

    void assertPeersMerged(const PeerWrapper& one, const PeerWrapper& two)
    {
        while (!PeerWrapper::allPeersHaveSameTransactionLog(
            std::vector<const PeerWrapper*>{&one, &two}))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void waitUntilEveryServerSeesEveryoneElseAsOnline()
    {
        for (;;)
        {
            bool foundOfflineServer = false;

            for (int i = 0; i < peerCount(); ++i)
            {
                nx::vms::api::MediaServerDataExList mediaServers;
                ASSERT_EQ(
                    ec2::ErrorCode::ok,
                    peer(i).mediaServerClient()->ec2GetMediaServersEx(&mediaServers));

                ASSERT_EQ(peerCount(), (int) mediaServers.size());
                for (const auto& server: mediaServers)
                {
                    if (server.status == nx::vms::api::ResourceStatus::offline)
                        foundOfflineServer = true;
                }
            }

            if (!foundOfflineServer)
                return;

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
};

TEST_F(SystemMerge, two_systems_can_be_merged)
{
    givenTwoSingleServerSystems();

    mergeSystems();

    thenMergeSucceeded();

    nx::vms::api::ResourceParamDataList peer0Settings;
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        peer(0).mediaServerClient()->ec2GetSettings(&peer0Settings));

    nx::vms::api::ResourceParamDataList peer1Settings;
    ASSERT_EQ(
        ec2::ErrorCode::ok,
        peer(1).mediaServerClient()->ec2GetSettings(&peer1Settings));

    ASSERT_TRUE(peer0Settings == peer1Settings);

    waitUntilAllServersSynchronizedData();
}

TEST_F(SystemMerge, merge_history_record_is_added_during_merge)
{
    givenTwoSingleServerSystems();
    mergeSystems();
    thenMergeHistoryRecordIsAdded();
}

TEST_F(SystemMerge, three_systems_merge_wierd_scenario)
{
    initializeSingleServerSystems(3);

    merge(peer(0), peer(1), {MergeAttributes::TakeRemoteSettings(true)});
    assertPeersMerged(peer(0), peer(1));

    merge(peer(2), peer(1), {MergeAttributes::TakeRemoteSettings(false)});
    assertPeersMerged(peer(2), peer(1));

    waitUntilAllServersSynchronizedData();
    waitUntilEveryServerSeesEveryoneElseAsOnline();
}

TEST_F(SystemMerge, cannot_merge_systems_with_servers_with_same_id)
{
    givenTwoSystemsWithServersWithSameId();
    mergeSystems();
    thenMergedFailed();
}

} // namespace test
} // namespace ec2
