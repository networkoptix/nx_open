#include <set>

#include <gtest/gtest.h>

#include <nx/cloud/cdb/managers/system_health_info_provider.h>

#include "test_setup.h"

namespace nx {
namespace cdb {
namespace test {

namespace {

class SystemHealthInfoProviderStub:
    public AbstractSystemHealthInfoProvider
{
public:
    virtual bool isSystemOnline(const std::string& systemId) const override
    {
        return m_onlineSystems.find(systemId) != m_onlineSystems.end();
    }

    virtual void getSystemHealthHistory(
        const AuthorizationInfo& /*authzInfo*/,
        data::SystemId /*systemId*/,
        std::function<void(api::ResultCode, api::SystemHealthHistory)> completionHandler) override
    {
        m_aioThreadBinder.post(
            std::bind(completionHandler, api::ResultCode::ok, api::SystemHealthHistory()));
    }

    void forceSystemOnline(const std::string& systemId)
    {
        m_onlineSystems.insert(systemId);
    }

private:
    nx::network::aio::BasicPollable m_aioThreadBinder;
    std::set<std::string> m_onlineSystems;
};

} // namespace

class SystemMerge:
    public CdbFunctionalTest
{
    using base_type = CdbFunctionalTest;

public:
    SystemMerge()
    {
        using namespace std::placeholders;

        m_factoryBak = SystemHealthInfoProviderFactory::instance().setCustomFunc(
            std::bind(&SystemMerge::createSystemHealthInfoProvider, this, _1, _2));
    }

    ~SystemMerge()
    {
        SystemHealthInfoProviderFactory::instance().setCustomFunc(std::move(m_factoryBak));
    }

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

    void givenTwoOnlineSystemsWithSameOwner()
    {
        givenTwoSystemsWithSameOwner();

        m_systemHealthInfoProviderStub->forceSystemOnline(m_masterSystem.id);
        m_systemHealthInfoProviderStub->forceSystemOnline(m_slaveSystem.id);
    }

    void givenTwoOnlineSystemsWithDifferentOwners()
    {
        givenTwoOnlineSystemsWithSameOwner();

        AccountWithPassword otherAccount = addActivatedAccount2();
        m_slaveSystem = addRandomSystemToAccount(otherAccount);

        m_systemHealthInfoProviderStub->forceSystemOnline(m_slaveSystem.id);
    }

    void whenMergeSystems()
    {
        m_prevResultCode = mergeSystems(m_ownerAccount, m_masterSystem.id, m_slaveSystem.id);
    }

    void thenResultCodeIs(api::ResultCode resultCode)
    {
        ASSERT_EQ(resultCode, m_prevResultCode);
    }

    void thenMergeSucceeded()
    {
        thenResultCodeIs(api::ResultCode::ok);
    }

    void andSlaveSystemIsMovedToBeingMergedState()
    {
        api::SystemDataEx system;
        ASSERT_EQ(
            api::ResultCode::ok,
            getSystem(m_ownerAccount.email, m_ownerAccount.password, m_slaveSystem.id, &system));
        ASSERT_EQ(api::SystemStatus::beingMerged, system.status);
    }

    void assertAnyOfPermissionsIsNotEnoughToMergeSystems(
        std::vector<api::SystemAccessRole> accessRolesToCheck)
    {
        for (const auto accessRole: accessRolesToCheck)
        {
            auto user = addActivatedAccount2();
            shareSystemEx(m_ownerAccount, m_masterSystem, user.email, accessRole);
            shareSystemEx(m_ownerAccount, m_slaveSystem, user.email, accessRole);

            ASSERT_EQ(
                api::ResultCode::forbidden,
                mergeSystems(user, m_masterSystem.id, m_slaveSystem.id));
        }
    }

private:
    AccountWithPassword m_ownerAccount;
    api::SystemData m_masterSystem;
    api::SystemData m_slaveSystem;
    api::ResultCode m_prevResultCode = api::ResultCode::ok;
    SystemHealthInfoProviderStub* m_systemHealthInfoProviderStub = nullptr;
    SystemHealthInfoProviderFactory::Function m_factoryBak;

    std::unique_ptr<AbstractSystemHealthInfoProvider> createSystemHealthInfoProvider(
        ec2::ConnectionManager*,
        nx::utils::db::AsyncSqlQueryExecutor*)
    {
        auto systemHealthInfoProvider = std::make_unique<SystemHealthInfoProviderStub>();
        m_systemHealthInfoProviderStub = systemHealthInfoProvider.get();
        return systemHealthInfoProvider;
    }
};

TEST_F(SystemMerge, merge_request_moves_slave_system_to_beingMerged_state)
{
    givenTwoOnlineSystemsWithSameOwner();

    whenMergeSystems();

    thenMergeSucceeded();
    andSlaveSystemIsMovedToBeingMergedState();
}

TEST_F(SystemMerge, fails_if_either_system_is_offline)
{
    givenTwoSystemsWithSameOwner();
    whenMergeSystems();
    thenResultCodeIs(api::ResultCode::mergedSystemIsOffline);
}

// TEST_F(SystemMerge, fails_if_either_system_is_older_than_3_2)

TEST_F(SystemMerge, fails_if_systems_have_different_owners)
{
    givenTwoOnlineSystemsWithDifferentOwners();
    whenMergeSystems();
    thenResultCodeIs(api::ResultCode::forbidden);
}

TEST_F(SystemMerge, fails_if_initiating_user_does_not_have_permissions)
{
    const std::vector<api::SystemAccessRole> accessRolesToTest{
        api::SystemAccessRole::disabled,
        api::SystemAccessRole::custom,
        api::SystemAccessRole::liveViewer,
        api::SystemAccessRole::viewer,
        api::SystemAccessRole::advancedViewer,
        api::SystemAccessRole::localAdmin,
        api::SystemAccessRole::cloudAdmin,
        api::SystemAccessRole::maintenance};

    givenTwoOnlineSystemsWithSameOwner();
    assertAnyOfPermissionsIsNotEnoughToMergeSystems(accessRolesToTest);
}

// TEST_F(SystemMerge, merge_request_invokes_merge_request_to_slave_system)
// TEST_F(SystemMerge, fails_if_request_to_slave_system_fails)

} // namespace test
} // namespace cdb
} // namespace nx
