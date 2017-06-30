#include <gtest/gtest.h>
#include <nx/casssandra/cassandra_connection.h>

namespace nx {
namespace cassandra {
namespace test {

using namespace nx::utils;

class Connection: public ::testing::Test
{
protected:
    Connection(): m_connection("127.0.0.1") {}

    void whenConnectionIsEstablished()
    {
        m_connection.initAsync([this](CassError result) { m_connectCb(result); });
    }

    void givenQueringChainOfCreateInsertRequestsSetAsConnectCb()
    {
        m_connectCb =
            [this](CassError result)
            {
                ASSERT_EQ(CASS_OK, result);
                createKeySpace();
            };

        m_createKeySpaceCb =
            [this](CassError result)
            {
                ASSERT_EQ(CASS_OK, result);
                createTable();
            };
    }

    void givenSelectRequestSetAsInsertCb()
    {

    }

    void thenInsertedDataShouldBeEqualToSelected()
    {

    }

private:
    nx::cassandra::Connection m_connection;
    MoveOnlyFunc<void(CassError)> m_connectCb;
    MoveOnlyFunc<void(CassError)> m_createKeySpaceCb;

    void createKeySpace() {}
    void createTable() {}

};

TEST_F(Connection, InsertSelect_Async)
{
    givenQueringChainOfCreateInsertRequestsSetAsConnectCb();
    givenSelectRequestSetAsInsertCb();
    whenConnectionIsEstablished();

    thenInsertedDataShouldBeEqualToSelected();
}

} // namespace test
} // namespace cassandra
} // namespace nx
