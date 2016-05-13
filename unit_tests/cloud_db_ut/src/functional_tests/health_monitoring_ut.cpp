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
    constexpr const std::size_t kMediaServersToEmulateCount = 2;

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

    //checking that cdb considers system as offline
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchSystemData(account1.email, account1Password, system1.id, &systemData));
    ASSERT_EQ(api::SystemHealth::offline, systemData.stateOfHealth);

    std::vector<std::unique_ptr<api::EventConnection>>
        eventConnections(kMediaServersToEmulateCount);

    //creating event connection
    for (auto& eventConnection: eventConnections)
    {
        eventConnection = connectionFactory()->createEventConnection(
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
        ASSERT_EQ(
            api::ResultCode::ok,
            fetchSystemData(account1.email, account1Password, system1.id, &systemData));
        ASSERT_EQ(api::SystemHealth::online, systemData.stateOfHealth);
    }

    //breaking connection
    for (auto& eventConnection : eventConnections)
    {
        ASSERT_EQ(
            api::ResultCode::ok,
            fetchSystemData(account1.email, account1Password, system1.id, &systemData));
        ASSERT_EQ(api::SystemHealth::online, systemData.stateOfHealth);
        eventConnection.reset();
    }

    //checking that cdb considers system as offline
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchSystemData(account1.email, account1Password, system1.id, &systemData));
    ASSERT_EQ(api::SystemHealth::offline, systemData.stateOfHealth);
}

TEST_F(HealthMonitoring, reconnect)
{
    addArg("-eventManager/mediaServerConnectionIdlePeriod");
    addArg("5s");

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

    std::this_thread::sleep_for(std::chrono::minutes(1));

    //checking that cdb considers system as online
    api::SystemDataEx systemData;
    ASSERT_EQ(
        api::ResultCode::ok,
        fetchSystemData(account1.email, account1Password, system1.id, &systemData));
    ASSERT_EQ(api::SystemHealth::online, systemData.stateOfHealth);

    eventConnection.reset();
}

}   //namespace cdb
}   //namespace nx
