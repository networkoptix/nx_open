#include <memory>

#include <gtest/gtest.h>

#include <nx/network/system_socket.h>
#include <nx/utils/std/cpp14.h>

#include "functional_tests/test_setup.h"
#include "functional_tests/system_ut.h"

namespace nx {
namespace cdb {
namespace test {

class CdbLoadTest:
    public CdbFunctionalTest
{
};

// Disabled since not ready yet.
TEST_F(CdbLoadTest, DISABLED_manyConnections)
{
    constexpr const int connectionCount = 10000;
    constexpr const int sendTimeoutMs = 3000;

    ASSERT_TRUE(startAndWaitUntilStarted());

    std::atomic<int> connectionsSucceeded(0);
    std::atomic<int> connectionsFailed(0);
    auto onConnectDone =
        [&connectionsSucceeded, &connectionsFailed](SystemError::ErrorCode errorCode)
        {
            //ASSERT_EQ(SystemError::noError, errorCode);
            if (errorCode == SystemError::noError)
                ++connectionsSucceeded;
            else
                ++connectionsFailed;
        };

    std::vector<std::unique_ptr<nx::network::TCPSocket>> connections(connectionCount);
    for (size_t i = 0; i < connections.size(); ++i)
    {
        auto& connection = connections[i];

        connection = std::make_unique<nx::network::TCPSocket>(AF_INET);
        ASSERT_TRUE(connection->setNonBlockingMode(true));
        ASSERT_TRUE(connection->setSendTimeout(sendTimeoutMs));
        connection->connectAsync(endpoint(), onConnectDone);

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    cdbFunctionalTestSystemGet(this);
}

#if 0
class TestScenario
:
    public QnStoppableAsync
{
public:
    virtual void pleaseStop(nx::utils::MoveOnlyFunc<void()> completionHandler);

    virtual void run(nx::utils::MoveOnlyFunc<void()> completionHandler);
};

TEST_F(CdbLoadTest, manyClients)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    TestScenario scenario;
    scenario.addStep(
        []{
            api::AccountData account1;
            std::string account1Password;
            ASSERT_EQ(
                api::ResultCode::ok,
                testSetup->addActivatedAccount(&account1, &account1Password));
            return std::make_pair(account1, account1Password);
        });

    scenario.addStep(
        [std::make_pair(account1, account1Password)]{
            //adding system2 to account1
            api::SystemData system1;
            ASSERT_EQ(
                api::ResultCode::ok,
                testSetup->bindRandomSystem(account1.email, account1Password, &system1));
        });

    scenario.addStep(
        []{
            std::vector<api::SystemDataEx> systems;
            ASSERT_EQ(
                api::ResultCode::ok,
                testSetup->getSystem(account1.email, account1Password, system1.id, &systems));
            ASSERT_EQ(1, systems.size());
            ASSERT_TRUE(std::find(systems.begin(), systems.end(), system1) != systems.end());
            ASSERT_EQ(account1.fullName, systems[0].ownerFullName);
        });

    //TODO #ak
}
#endif

} // namespace cdb
} // namespace cdb
} // namespace nx
