#include <gtest/gtest.h>

#include <nx/client/desktop/system_administration/redux/time_syncronization_widget_state.h>
#include <nx/client/desktop/system_administration/redux/time_syncronization_widget_state_reducer.h>

namespace nx::client::desktop::test {

class TimeSynchronizationWidgetReducerTest: public testing::Test
{
public:
    using State = TimeSynchronizationWidgetState;
    using Reducer = TimeSynchronizationWidgetReducer;

    State::ServerInfo server(const QnUuid& id = QnUuid())
    {
        State::ServerInfo server;
        server.id = id.isNull() ? QnUuid::createUuid() : id;
        return server;
    }
};

// Check default status for the system, connected to the Internet.
TEST_F(TimeSynchronizationWidgetReducerTest, synchronizedWithInternet)
{
    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server()});

    ASSERT_EQ(s.status, State::Status::synchronizedWithInternet);
}

// Check default status for the system with primary time server set.
TEST_F(TimeSynchronizationWidgetReducerTest, synchronizedWithSelectedServer)
{
    const auto id = QnUuid::createUuid();

    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    ASSERT_EQ(s.status, State::Status::synchronizedWithSelectedServer);
    ASSERT_EQ(s.actualPrimaryServer(), id);
}

// Check default status when synchronization is disabled at all.
TEST_F(TimeSynchronizationWidgetReducerTest, notSynchronizedByDisabledFlag)
{
    const auto id = QnUuid::createUuid();

    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ false,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    ASSERT_EQ(s.status, State::Status::notSynchronized);
    ASSERT_TRUE(s.actualPrimaryServer().isNull());
}

TEST_F(TimeSynchronizationWidgetReducerTest, notSynchronizedByNullServer)
{
    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server(), server()});

    ASSERT_EQ(s.status, State::Status::notSynchronized);
}

TEST_F(TimeSynchronizationWidgetReducerTest, notSynchronizedByInvalidServer)
{
    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid::createUuid(),
        /*servers*/ {server(), server()});

    ASSERT_EQ(s.status, State::Status::notSynchronized);
    ASSERT_TRUE(s.actualPrimaryServer().isNull());
}

// Synchronization is enabled via primary time server; there is only one server in the system.
TEST_F(TimeSynchronizationWidgetReducerTest, singleServerLocalTimeCorrectId)
{
    const auto id = QnUuid::createUuid();

    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id)});

    ASSERT_EQ(s.status, State::Status::singleServerLocalTime);
    ASSERT_EQ(s.actualPrimaryServer(), id);
}

// Empty primary time server actually means internet synchronization.
TEST_F(TimeSynchronizationWidgetReducerTest, singleServerLocalTimeEmptyId)
{
    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server()});

    ASSERT_EQ(s.status, State::Status::synchronizedWithInternet);
}

// Empty primary time server actually means internet synchronization.
TEST_F(TimeSynchronizationWidgetReducerTest, singleServerLocalTimeInvalidId)
{
    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid::createUuid(),
        /*servers*/ {server()});

    ASSERT_EQ(s.status, State::Status::synchronizedWithInternet);
    ASSERT_TRUE(s.actualPrimaryServer().isNull());
}

// Default setup when internet synchronization is enabled but no servers have internet.
TEST_F(TimeSynchronizationWidgetReducerTest, noInternetConnection)
{
    auto srv = server();
    srv.hasInternet = false;

    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {srv});

    ASSERT_EQ(s.status, State::Status::noInternetConnection);
}

// Default setup when selected time server is offline.
TEST_F(TimeSynchronizationWidgetReducerTest, selectedServerIsOffline)
{
    const auto id = QnUuid::createUuid();
    auto srv = server(id);
    srv.online = false;

    const auto s = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {srv, server()});

    ASSERT_EQ(s.status, State::Status::selectedServerIsOffline);
    ASSERT_EQ(s.actualPrimaryServer(), id);
}

} // namespace nx::client::desktop::test
