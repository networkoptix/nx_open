#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <nx/utils/random.h>
#include <nx_ec/ec_api.h>

#include <utils/common/sync_call.h>

#include "ec2/cloud_vms_synchro_test_helper.h"
#include "test_setup.h"

namespace nx {
namespace cdb {

#if 1

class HealthMonitoring:
    public Ec2MserverCloudSynchronization
{
};

TEST_F(HealthMonitoring, general)
{
    ASSERT_TRUE(cdb()->startAndWaitUntilStarted());
    ASSERT_TRUE(appserver2()->startAndWaitUntilStarted());
    ASSERT_EQ(api::ResultCode::ok, registerAccountAndBindSystemToIt());

    for (int i = 0; i < 2; ++i)
    {
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->fetchSystemData(
                ownerAccount().email, ownerAccount().password,
                registeredSystemData().id, &systemData));
        ASSERT_EQ(api::SystemHealth::offline, systemData.stateOfHealth);

        appserver2()->moduleInstance()->ecConnection()->addRemotePeer(cdbEc2TransactionUrl());

        waitForCloudAndVmsToSyncUsers();

        ASSERT_EQ(
            api::ResultCode::ok,
            cdb()->fetchSystemData(
                ownerAccount().email, ownerAccount().password,
                registeredSystemData().id, &systemData));
        ASSERT_EQ(api::SystemHealth::online, systemData.stateOfHealth);

        if (i == 0)
        {
            appserver2()->moduleInstance()->ecConnection()->deleteRemotePeer(cdbEc2TransactionUrl());
            ASSERT_TRUE(cdb()->restart());
        }
    }
}


#else
class HealthMonitoring:
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
    addArg("3s");

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

    const auto t1 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t1
        < std::chrono::seconds(utils::random::number(8, 10)))
    {
        //checking that cdb considers system as online
        api::SystemDataEx systemData;
        ASSERT_EQ(
            api::ResultCode::ok,
            fetchSystemData(account1.email, account1Password, system1.id, &systemData));
        ASSERT_EQ(api::SystemHealth::online, systemData.stateOfHealth);

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    eventConnection.reset();
}

TEST_F(HealthMonitoring, badCredentials)
{
    ASSERT_TRUE(startAndWaitUntilStarted());

    api::ResultCode result = api::ResultCode::ok;

    for (int i = 0; i < 2; ++i)
    {
        std::unique_ptr<nx::cdb::api::EventConnection> eventConnection;

        if (i == 0)
            eventConnection = connectionFactory()->createEventConnection(
                "bla-bla-bla",
                "be-be-be");
        else
            eventConnection = connectionFactory()->createEventConnection(
                "",
                "");

        api::SystemEventHandlers eventHandlers;
        std::tie(result) = makeSyncCall<api::ResultCode>(
            std::bind(
                &nx::cdb::api::EventConnection::start,
                eventConnection.get(),
                std::move(eventHandlers),
                std::placeholders::_1));
        ASSERT_EQ(api::ResultCode::notAuthorized, result);
    }

    //currently, events are for systems only

    api::AccountData account1;
    std::string account1Password;
    result = addActivatedAccount(&account1, &account1Password);
    ASSERT_EQ(api::ResultCode::ok, result);

    auto eventConnection = connectionFactory()->createEventConnection(
        account1.email,
        account1Password);

    api::SystemEventHandlers eventHandlers;
    std::tie(result) = makeSyncCall<api::ResultCode>(
        std::bind(
            &nx::cdb::api::EventConnection::start,
            eventConnection.get(),
            std::move(eventHandlers),
            std::placeholders::_1));
    ASSERT_EQ(api::ResultCode::forbidden, result);
}
#endif

}   //namespace cdb
}   //namespace nx
