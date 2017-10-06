#include <gtest/gtest.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

class SystemMerge:
    public CdbFunctionalTest
{
    using base_type = CdbFunctionalTest;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();

        ASSERT_TRUE(startAndWaitUntilStarted());

        m_ownerAccount = addActivatedAccount2();
    }

    void givenTwoSystemsWithSameOwner()
    {
        m_masterSystem = addRandomSystemToAccount(m_ownerAccount);
        m_slaveSystem = addRandomSystemToAccount(m_ownerAccount);
    }

    void whenMergeSystems()
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            mergeSystems(m_ownerAccount, m_masterSystem.id, m_slaveSystem.id));
    }

    void thenSlaveSystemIsMovedToBeingMergedState()
    {
        api::SystemDataEx system;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(m_ownerAccount.email, m_ownerAccount.password, m_slaveSystem.id, &system));
        ASSERT_EQ(api::SystemStatus::beingMerged, system.status);
    }

private:
    AccountWithPassword m_ownerAccount;
    api::SystemData m_masterSystem;
    api::SystemData m_slaveSystem;
};

TEST_F(SystemMerge, merge_request_moves_slave_system_to_beingMerged_state)
{
    givenTwoSystemsWithSameOwner();
    whenMergeSystems();
    thenSlaveSystemIsMovedToBeingMergedState();
}

// TEST_F(SystemMerge, merge_request_invokes_merge_request_to_slave_system)
// TEST_F(SystemMerge, fails_if_either_system_is_offline)
// TEST_F(SystemMerge, fails_if_either_system_is_older_than_3_2)
// TEST_F(SystemMerge, fails_if_systems_have_different_owners)
// TEST_F(SystemMerge, fails_if_initiating_user_does_not_have_permissions)
// TEST_F(SystemMerge, fails_if_request_to_slave_system_fails)

} // namespace test
} // namespace cdb
} // namespace nx
