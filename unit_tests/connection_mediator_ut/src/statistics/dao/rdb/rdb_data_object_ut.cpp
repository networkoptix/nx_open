#include <gtest/gtest.h>

#include <nx/utils/test_support/utils.h>

#include <utils/db/db_connection_holder.h>
#include <utils/db/test_support/test_with_db_helper.h>

#include <statistics/dao/rdb/rdb_data_object.h>

namespace nx {
namespace hpm {
namespace stats {
namespace dao {
namespace rdb {
namespace test {

class RdbDataObject:
    public db::test::TestWithDbHelper,
    public ::testing::Test
{
public:
    RdbDataObject():
        db::test::TestWithDbHelper("hpm", QString()),
        m_dbConnection(dbConnectionOptions())
    {
        init();
    }

protected:
    void havingSavedRecordsToDb()
    {
        ASSERT_TRUE(false);
    }

    void havingReinitializedDao()
    {
    }

    void assertIfRecordsWereNotSaved()
    {
    }

private:
    rdb::RdbDataObject m_dao;
    nx::db::DbConnectionHolder m_dbConnection;

    void init()
    {
        ASSERT_TRUE(m_dbConnection.open());
    }
};

TEST_F(RdbDataObject, what_is_saved_that_can_be_restored)
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
