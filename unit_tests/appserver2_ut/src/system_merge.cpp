#include <memory>

#include <gtest/gtest.h>

#include <QtCore/QDir>

#include <nx/network/app_info.h>
#include <nx/network/http/auth_tools.h>
#include <nx/network/url/url_builder.h>
#include <nx/utils/test_support/module_instance_launcher.h>
#include <nx/utils/test_support/test_options.h>
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
    public SystemMergeFixture
{
protected:
    void givenTwoSingleServerSystems()
    {
        ASSERT_TRUE(initializeSingleServerSystems(2));
    }

    void thenMergeSucceeded()
    {
        ASSERT_EQ(QnRestResult::Error::NoError, prevMergeResult());
    }

    void thenMergeHistoryRecordIsAdded()
    {
        waitUntilMergeHistoryIsAdded();
    }
};

TEST_F(SystemMerge, two_systems_can_be_merged)
{
    givenTwoSingleServerSystems();

    mergeSystems();

    thenMergeSucceeded();
    waitUntilAllServersSynchronizedData();
}

TEST_F(SystemMerge, merge_history_record_is_added_during_merge)
{
    givenTwoSingleServerSystems();
    mergeSystems();
    thenMergeHistoryRecordIsAdded();
}

} // namespace test
} // namespace ec2
