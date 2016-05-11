/**********************************************************
* May 11, 2016
* akolesnikov
***********************************************************/

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utils/common/sync_call.h>

#include "test_setup.h"


namespace nx {
namespace cdb {

class HealthMonitoring
:
    public CdbFunctionalTest
{
};

TEST_F(HealthMonitoring, general)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    //adding system1 to account1
    api::SystemData system1;
    result = bindRandomSystem(account1.email, account1Password, &system1);
    ASSERT_EQ(api::ResultCode::ok, result);

    //creating event connection
    auto eventConnection = connectionFactory()->createEventConnection(
        system1.id,
        system1.authKey);

    api::SystemEventHandlers eventHandlers;
    std::tie(result) = makeSyncCall<api::ResultCode>(
        std::bind(
            &nx::cdb::api::EventConnection::start,
            eventConnection.get(),
            std::move(eventHandlers),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::ok, result);

    //checking that cdb considers system as online
    std::vector<api::SystemDataEx> systems;
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account1.email, account1Password, &systems));
    ASSERT_EQ(1, systems.size());   //only activated systems are provided
    ASSERT_EQ(api::SystemHealth::online, systems[0].stateOfHealth);

    //breaking connection
    eventConnection.reset();

    //checking that cdb considers system as offline
    systems.clear();
    ASSERT_EQ(
        api::ResultCode::ok,
        getSystems(account1.email, account1Password, &systems));
    ASSERT_EQ(1, systems.size());   //only activated systems are provided
    ASSERT_EQ(api::SystemHealth::offline, systems[0].stateOfHealth);
}

}   //namespace cdb
}   //namespace nx
