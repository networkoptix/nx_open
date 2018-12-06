#include <gtest/gtest.h>

#include <nx/vms/client/desktop/system_administration/redux/time_synchronization_widget_state.h>
#include <nx/vms/client/desktop/system_administration/redux/time_synchronization_widget_state_reducer.h>

namespace nx::vms::client::desktop {

using State = TimeSynchronizationWidgetState;
using Reducer = TimeSynchronizationWidgetReducer;

void PrintTo(const State::Status& val, ::std::ostream* os)
{
    QString result;
    switch (val)
    {
        case State::Status::synchronizedWithInternet:
            result = "synchronizedWithInternet";
            break;
        case State::Status::synchronizedWithSelectedServer:
            result = "synchronizedWithSelectedServer";
            break;
        case State::Status::notSynchronized:
            result = "notSynchronized";
            break;
        case State::Status::singleServerLocalTime:
            result = "singleServerLocalTime";
            break;
        case State::Status::noInternetConnection:
            result = "noInternetConnection";
            break;
        case State::Status::selectedServerIsOffline:
            result = "selectedServerIsOffline";
            break;
    }
    *os << result.toStdString();
}

namespace test {

class TimeSynchronizationWidgetReducerTest: public testing::Test
{
public:
    static State::ServerInfo server(const QnUuid& id = QnUuid())
    {
        State::ServerInfo server;
        server.id = id.isNull() ? QnUuid::createUuid() : id;
        return server;
    }

    void assertIsActual(const State& state) const
    {
        // Validate state self-consistency.
        ASSERT_EQ(state.primaryServer, Reducer::actualPrimaryServer(state).id);
        ASSERT_EQ(state.status, Reducer::actualStatus(state));
        if (state.syncTimeWithInternet())
        {
            ASSERT_TRUE(state.enabled);
            ASSERT_TRUE(state.primaryServer.isNull());
        }
        else if (state.status == State::Status::notSynchronized)
        {
            ASSERT_FALSE(state.enabled);
            ASSERT_TRUE(state.primaryServer.isNull());
        }
        else
        {
            ASSERT_TRUE(state.enabled);
            ASSERT_FALSE(state.primaryServer.isNull());
        }
    }
};

// Synchronization is enabled via internet.
TEST_F(TimeSynchronizationWidgetReducerTest, synchronizedWithInternet)
{
    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::synchronizedWithInternet);
}

// Default setup when internet synchronization is enabled but no servers have internet.
TEST_F(TimeSynchronizationWidgetReducerTest, noInternetConnection)
{
    auto srv = server();
    srv.hasInternet = false;

    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {srv});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::noInternetConnection);
}

// Synchronization is enabled via primary time server.
TEST_F(TimeSynchronizationWidgetReducerTest, synchronizedWithSelectedServer)
{
    const auto id = QnUuid::createUuid();

    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::synchronizedWithSelectedServer);
}

// Synchronization is enabled via primary time server, which is offline right now.
TEST_F(TimeSynchronizationWidgetReducerTest, selectedServerIsOffline)
{
    const auto id = QnUuid::createUuid();
    auto srv = server(id);
    srv.online = false;
    srv.hasInternet = false;

    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {srv, server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::selectedServerIsOffline);
}

// Synchronization is enabled via primary time server; there is only one server in the system.
TEST_F(TimeSynchronizationWidgetReducerTest, singleServerLocalTime)
{
    const auto id = QnUuid::createUuid();

    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id)});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::singleServerLocalTime);
}

TEST_F(TimeSynchronizationWidgetReducerTest, notSynchronizedByInvalidServer)
{
    const auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid::createUuid(),
        /*servers*/ {server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::notSynchronized);
}

// Synchronization is disabled at all. All other options are ignored.
TEST_F(TimeSynchronizationWidgetReducerTest, notSynchronizedByDisabledFlag)
{
    // Check with a server.
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ false,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::notSynchronized);

    // Check with an internet sync.
    state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ false,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server(id), server()});

    assertIsActual(state);
    ASSERT_EQ(state.status, State::Status::notSynchronized);
}

// When there is only one server, disabling internet sync should select it as a primary server.
TEST_F(TimeSynchronizationWidgetReducerTest, disableSyncWithInternetWithSingleServer)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server(id)});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::setSyncTimeWithInternet(std::move(state), false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(result.primaryServer, id);
    ASSERT_EQ(result.status, State::Status::singleServerLocalTime);
}

// When there are some servers, disabling internet sync should disable sync completely.
TEST_F(TimeSynchronizationWidgetReducerTest, disableSyncWithInternetWithManyServers)
{
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server(), server()});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::setSyncTimeWithInternet(std::move(state), false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(result.status, State::Status::notSynchronized);
}

// Enabling and disabling internet sync must keep the selected time server.
TEST_F(TimeSynchronizationWidgetReducerTest, keepOldSelectedServer)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    bool changed = false;
    State afterEnabled;
    std::tie(changed, afterEnabled) = Reducer::setSyncTimeWithInternet(std::move(state), true);
    ASSERT_TRUE(changed);
    ASSERT_EQ(afterEnabled.status, State::Status::synchronizedWithInternet);

    State result;
    std::tie(changed, result) = Reducer::setSyncTimeWithInternet(std::move(afterEnabled), false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(result.primaryServer, id);
    ASSERT_EQ(result.status, State::Status::synchronizedWithSelectedServer);
}

// Disabling sync when sync with internet
TEST_F(TimeSynchronizationWidgetReducerTest, disableSyncWhenSyncedWithInternet)
{
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server()});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::disableSync(std::move(state));
    ASSERT_TRUE(changed);
    ASSERT_FALSE(result.enabled);
    ASSERT_TRUE(result.primaryServer.isNull());
    ASSERT_EQ(result.status, State::Status::notSynchronized);
}

// Disable sync must keep last primary server
TEST_F(TimeSynchronizationWidgetReducerTest, disableSyncWhenServerSelected)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id)});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::disableSync(std::move(state));
    ASSERT_TRUE(changed);
    ASSERT_FALSE(result.enabled);
    ASSERT_TRUE(result.primaryServer.isNull());
    ASSERT_EQ(result.lastPrimaryServer, id);
    ASSERT_EQ(result.status, State::Status::notSynchronized);
}

// Enabling and disabling internet sync after sync was disabled must clear selected time server.
TEST_F(TimeSynchronizationWidgetReducerTest, clearOldSelectedServerAfterSyncWasDisabled)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    bool changed = false;

    State afterDisabled;
    std::tie(changed, afterDisabled) = Reducer::disableSync(std::move(state));
    ASSERT_TRUE(changed);
    ASSERT_EQ(afterDisabled.lastPrimaryServer, id);
    ASSERT_EQ(afterDisabled.status, State::Status::notSynchronized);

    State afterEnabled;
    std::tie(changed, afterEnabled) = Reducer::setSyncTimeWithInternet(std::move(afterDisabled), true);
    ASSERT_TRUE(changed);
    ASSERT_TRUE(afterEnabled.lastPrimaryServer.isNull());
    ASSERT_EQ(afterEnabled.status, State::Status::synchronizedWithInternet);

    State result;
    std::tie(changed, result) = Reducer::setSyncTimeWithInternet(std::move(afterEnabled), false);
    ASSERT_TRUE(changed);
    ASSERT_EQ(result.status, State::Status::notSynchronized);
}

TEST_F(TimeSynchronizationWidgetReducerTest, selectServerWhenInternetSync)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid::createUuid(),
        /*servers*/ {server(), server(id)});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::selectServer(std::move(state), id);

    assertIsActual(result);
    ASSERT_EQ(result.primaryServer, id);
    ASSERT_EQ(result.status, State::Status::synchronizedWithSelectedServer);
}

TEST_F(TimeSynchronizationWidgetReducerTest, selectServerWhenWasDisabled)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ false,
        /*primaryTimeServer*/ QnUuid(),
        /*servers*/ {server(), server(id)});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::selectServer(std::move(state), id);

    assertIsActual(result);
    ASSERT_EQ(result.primaryServer, id);
    ASSERT_EQ(result.status, State::Status::synchronizedWithSelectedServer);
}

// Selecting invalid server must sync the system with the Internet.
TEST_F(TimeSynchronizationWidgetReducerTest, selectInvalidServer)
{
    const auto id = QnUuid::createUuid();
    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ QnUuid::createUuid(),
        /*servers*/ {server(), server()});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::selectServer(std::move(state), id);

    assertIsActual(result);
    ASSERT_TRUE(result.primaryServer.isNull());
    ASSERT_EQ(result.status, State::Status::synchronizedWithInternet);
}

TEST_F(TimeSynchronizationWidgetReducerTest, selectServerTwice)
{
    const auto id = QnUuid::createUuid();

    auto state = Reducer::initialize(State(),
        /*isTimeSynchronizationEnabled*/ true,
        /*primaryTimeServer*/ id,
        /*servers*/ {server(id), server()});

    bool changed = false;
    State result;
    std::tie(changed, result) = Reducer::selectServer(std::move(state), id);
    ASSERT_FALSE(changed);
}

} // namespace test
} // namespace nx::vms::client::desktop
