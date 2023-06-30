// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>

#include <nx/fusion/model_functions.h>

#include "analytics_settings_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api::analytics; //< TODO: Remove lexical reduplication.
using namespace nx::vms::common;

DeviceAgentSettingsResponse deserializedResponse(QByteArray data)
{
    bool success = true;
    const auto result =  QJson::deserialized<DeviceAgentSettingsResponse>(data, {}, &success);
    NX_ASSERT(success);
    return result;
}

// Such reply may be sent by the server when device agent is actually offline.
static const auto kEmptyReply = deserializedResponse(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {},
        "settingsValues": {}
    }
)json");

static const auto kData1Reply = deserializedResponse(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generateCars", "type": "CheckBox" } ],
            "type": "Settings"
        },
        "settingsValues": { "generateCars": true },
        "session": {
            "id": "{14eb242a-7ad7-4915-bfde-f681674db440}",
            "sequenceNumber": "1"
        }
    }
)json");

static const auto kData2Reply = deserializedResponse(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generatePersons", "type": "CheckBox" } ],
            "type": "Settings"
        },
        "settingsValues": { "generatePersons": false },
        "session": {
            "id": "{99a457f2-b469-411e-b3dc-f4fbd83b7381}",
            "sequenceNumber": "1"
        }
    }
)json");

class AnalyticsSettingsManagerTest: public AnalyticsSettingsTestFixture
{
    using base_type = AnalyticsSettingsTestFixture;

protected:
    virtual void SetUp() override
    {
        base_type::SetUp();
    }

    virtual void TearDown() override
    {
        m_notifier.reset();
        m_listener.reset();
        m_deviceAgentId.reset();
        base_type::TearDown();
    }

    core::DeviceAgentId deviceAgentId() const
    {
        return *m_deviceAgentId;
    }

    core::AnalyticsSettingsListenerPtr listener() const
    {
        return m_listener;
    }

    ListenerNotifier* notifier() const
    {
        return m_notifier.get();
    }

    void givenDeviceAgent()
    {
        auto camera = addCamera();
        auto engine = addEngine();
        m_deviceAgentId.reset(new core::DeviceAgentId{camera->getId(), engine->getId()});
        m_listener = manager()->getListener(deviceAgentId());
        m_notifier.reset(new ListenerNotifier(listener()));
    }

    DeviceAgentSettingsResponse givenDataOlderThan(const DeviceAgentSettingsResponse& other) const
    {
        DeviceAgentSettingsResponse data(other);
        data.session.sequenceNumber = other.session.sequenceNumber - 1;
        data.settingsModelId = QnUuid::createUuid(); //< Make sure this will cause model change.
        return data;
    }

    DeviceAgentSettingsResponse givenDataNewerThan(const DeviceAgentSettingsResponse& other) const
    {
        DeviceAgentSettingsResponse data(other);
        data.session.sequenceNumber = other.session.sequenceNumber + 1;
        data.settingsModelId = QnUuid::createUuid(); //< Make sure this will cause model change.
        return data;
    }

    void whenDataReceived(const DeviceAgentSettingsResponse& data)
    {
        m_serverInterfaceMock->sendReply(deviceAgentId(), data);
    }

    void whenReceivedTransactionUpdate(const DeviceAgentSettingsResponse& data)
    {
        m_manager->storeSettings(deviceAgentId(), data);
    }

    bool requestWasSent() const
    {
        return m_serverInterfaceMock->requestWasSent(deviceAgentId());
    }

private:
    QScopedPointer<core::DeviceAgentId> m_deviceAgentId;
    core::AnalyticsSettingsListenerPtr m_listener;
    QScopedPointer<ListenerNotifier> m_notifier;
};

/**
 * Validate VMS-17327: camera settings dialog did not get an update when a loaded model was empty.
 * Solution: add request status to a passed data, so dialog will know the status for sure.
 */
TEST_F(AnalyticsSettingsManagerTest, statusUpdateOnEmptyReply)
{
    givenDeviceAgent();
    // Check manager made a request here.
    EXPECT_TRUE(requestWasSent());

    // Check listener is still waiting.
    ASSERT_TRUE(listener()->data().status == core::DeviceAgentData::Status::loading);

    // Emulate network response.
    whenDataReceived(kEmptyReply);

    // Check listener sent actual data notification.
    ASSERT_EQ(notifier()->counter, 1);
    ASSERT_TRUE(listener()->data().status == core::DeviceAgentData::Status::ok);
}

TEST_F(AnalyticsSettingsManagerTest, listenToExternalChanges)
{
    givenDeviceAgent();
    whenDataReceived(kData1Reply);
    EXPECT_FALSE(requestWasSent());

    manager()->refreshSettings(deviceAgentId());
    ASSERT_TRUE(m_serverInterfaceMock->requestWasSent(deviceAgentId()));

    // Emulate network response.
    whenDataReceived(kData2Reply);

    // Check consumer received actual data.
    ASSERT_EQ(QJson::serialized(notifier()->lastData.model),
        QJson::serialized(kData2Reply.settingsModel));
    ASSERT_EQ(QJson::serialized(notifier()->lastData.values),
        QJson::serialized(kData2Reply.settingsValues));
}

// Update actual values on applying changes.
TEST_F(AnalyticsSettingsManagerTest, immediateUpdateOnApplyChanges)
{
    givenDeviceAgent();
    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 1); //< First update here.

    auto actualValues = kData1Reply.settingsValues;
    actualValues["generateCars"] = false;
    manager()->applyChanges({{deviceAgentId(), actualValues}});
    EXPECT_TRUE(requestWasSent());
    EXPECT_EQ(notifier()->counter, 2); //< Update immediately on changed values.
    ASSERT_EQ(QJson::serialized(listener()->data().values),
        QJson::serialized(actualValues));
}

// Explicit refresh must be handled
TEST_F(AnalyticsSettingsManagerTest, explicitRefreshCall)
{
    givenDeviceAgent();
    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 1); //< First update here.

    manager()->refreshSettings(deviceAgentId());
    EXPECT_EQ(notifier()->counter, 2); //< "Loading" status set.
    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 3); //< "Loading" status cleared.
}

// Refresh race: request before newer data was received.
TEST_F(AnalyticsSettingsManagerTest, refreshCallRace)
{
    givenDeviceAgent();
    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 1); //< First update here.

    manager()->refreshSettings(deviceAgentId());
    EXPECT_EQ(notifier()->counter, 2); //< "Loading" status set.

    const auto newerData = givenDataNewerThan(kData1Reply);
    whenReceivedTransactionUpdate(newerData);

    EXPECT_EQ(notifier()->counter, 3); //< "Loading" status cleared, newer data handled.

    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 3); //< Outdated response on our requested data was skipped.
}

TEST_F(AnalyticsSettingsManagerTest, checkSequenceNumber)
{
    givenDeviceAgent();
    whenDataReceived(kData1Reply);
    EXPECT_EQ(notifier()->counter, 1); //< First update here.

    const auto olderData = givenDataOlderThan(kData1Reply);
    whenReceivedTransactionUpdate(olderData);
    EXPECT_EQ(notifier()->counter, 1); //< No update was emitted.

    const auto newerData = givenDataNewerThan(kData1Reply);
    whenReceivedTransactionUpdate(newerData);
    EXPECT_EQ(notifier()->counter, 2); //< Newer model was processed.
}

} // namespace test
} // namespace nx::vms::client::desktop
