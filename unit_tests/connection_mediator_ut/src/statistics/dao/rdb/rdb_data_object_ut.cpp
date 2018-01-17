#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx/utils/test_support/utils.h>

#include <nx/utils/db/db_connection_holder.h>
#include <nx/utils/db/test_support/test_with_db_helper.h>

#include <statistics/dao/rdb/instance_controller.h>
#include <statistics/dao/rdb/rdb_data_object.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {
namespace test {

class StatisticsRdbDataObject:
    public nx::utils::db::test::TestWithDbHelper,
    public ::testing::Test
{
public:
    StatisticsRdbDataObject():
        nx::utils::db::test::TestWithDbHelper("hpm", QString())
    {
        init();
    }

protected:
    void havingSavedRecordsToDb()
    {
        generateRecords();
        auto queryContext = m_dbConnection->begin();
        for (const auto& statsRecord: m_records)
        {
            ASSERT_EQ(
                nx::utils::db::DBResult::ok,
                m_dao.save(queryContext.get(), statsRecord));
        }
        ASSERT_EQ(
            nx::utils::db::DBResult::ok,
            queryContext->transaction()->commit());
    }

    void havingReinitializedDao()
    {
        m_dbConnection.reset();
        m_dbInstance.reset();

        init();
    }

    void assertIfRecordsWereNotSaved()
    {
        const auto queryContext = m_dbConnection->begin();
        std::deque<stats::ConnectSession> readRecords;
        ASSERT_EQ(
            nx::utils::db::DBResult::ok,
            m_dao.readAllRecords(queryContext.get(), &readRecords));

        auto comparator =
            [](const stats::ConnectSession& one, const stats::ConnectSession& two)
            {
                return one.sessionId < two.sessionId;
            };
        std::sort(m_records.begin(), m_records.end(), comparator);
        std::sort(readRecords.begin(), readRecords.end(), comparator);

        ASSERT_EQ(m_records.size(), readRecords.size());
        ASSERT_EQ(m_records, readRecords);
    }

private:
    std::unique_ptr<nx::utils::db::DbConnectionHolder> m_dbConnection;
    std::unique_ptr<rdb::InstanceController> m_dbInstance;
    rdb::DataObject m_dao;
    std::deque<stats::ConnectSession> m_records;

    void init()
    {
        m_dbConnection = std::make_unique<nx::utils::db::DbConnectionHolder>(dbConnectionOptions());
        ASSERT_TRUE(m_dbConnection->open());

        ASSERT_NO_THROW(
            m_dbInstance = std::make_unique<rdb::InstanceController>(dbConnectionOptions()));
    }

    void generateRecords()
    {
        m_records.resize(7);
        for (auto& rec: m_records)
        {
            rec.startTime =
                std::chrono::system_clock::from_time_t(
                    nx::utils::random::number<int>(1, 1000000));
            rec.endTime =
                std::chrono::system_clock::from_time_t(
                    nx::utils::random::number<int>(1, 1000000));
            rec.resultCode =
                nx::hpm::api::NatTraversalResultCode::mediatorReportedError;
            rec.sessionId = QnUuid::createUuid().toSimpleByteArray();
            rec.originatingHostEndpoint =
                nx::network::SocketAddress(
                    QnUuid::createUuid().toSimpleString(),
                    nx::utils::random::number<int>(1, 10000)).toString().toUtf8();
            rec.originatingHostName = QnUuid::createUuid().toSimpleByteArray();
            rec.destinationHostEndpoint =
                nx::network::SocketAddress(
                    QnUuid::createUuid().toSimpleString(),
                    nx::utils::random::number<int>(1, 10000)).toString().toUtf8();
            rec.destinationHostName = QnUuid::createUuid().toSimpleByteArray();
        }
    }
};

TEST_F(StatisticsRdbDataObject, what_is_saved_that_can_be_restored)
{
    havingSavedRecordsToDb();
    havingReinitializedDao();
    assertIfRecordsWereNotSaved();
}

} // namespace test
} // namespace rdb
} // namespace dao
} // namespace stats
} // namespace hpm
} // namespace nx
