#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/test_support/test_options.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>

#include <test_support/peer_wrapper.h>

namespace ec2 {
namespace test {

class MiscManager:
    public nx::utils::test::TestWithTemporaryDirectory,
    public ::testing::Test
{
    using base_type = nx::utils::test::TestWithTemporaryDirectory;

public:
    MiscManager():
        base_type("MiscManager", nx::utils::TestOptions::temporaryDirectoryPath()),
        m_appserver2(nx::utils::TestOptions::temporaryDirectoryPath())
    {
    }

protected:
    void whenAddedMergeHistoryRecord()
    {
        m_originalRecords.push_back(ApiSystemMergeHistoryRecord());
        m_originalRecords.back().mergedSystemCloudId = QnUuid::createUuid().toSimpleByteArray();
        m_originalRecords.back().mergedSystemLocalId = QnUuid::createUuid().toSimpleByteArray();
        m_originalRecords.back().timestamp = nx::utils::random::number<int>();
        m_originalRecords.back().username = nx::utils::generateRandomName(7);
        m_originalRecords.back().signature = nx::utils::generateRandomName(7);
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            m_appserver2.ecConnection()->getMiscManager(Qn::kSystemAccess)->
                saveSystemMergeHistoryRecord(m_originalRecords.back()));
    }

    void whenAddedMultipleMergeHistoryRecord()
    {
        static const int recordsToAdd = 3;
        for (int i = 0; i < recordsToAdd; ++i)
            whenAddedMergeHistoryRecord();
    }
    
    void whenRestartedServer()
    {
        m_appserver2.process().restart();
    }

    void thenMergeHistoryCanBeRead()
    {
        ApiSystemMergeHistoryRecordList mergeHistory;
        ASSERT_EQ(
            ::ec2::ErrorCode::ok,
            m_appserver2.ecConnection()->getMiscManager(Qn::kSystemAccess)->
                getSystemMergeHistorySync(&mergeHistory));

        ASSERT_EQ(m_originalRecords.size(), mergeHistory.size());
    }

private:
    PeerWrapper m_appserver2;
    ApiSystemMergeHistoryRecordList m_originalRecords;

    virtual void SetUp() override
    {
        ASSERT_TRUE(m_appserver2.startAndWaitUntilStarted());
        ASSERT_TRUE(m_appserver2.configureAsLocalSystem());
    }
};

TEST_F(MiscManager, saved_merge_history_record_can_be_read_later)
{
    whenAddedMultipleMergeHistoryRecord();
    whenRestartedServer();

    thenMergeHistoryCanBeRead();
}

} // namespace test
} // namespace ec2
