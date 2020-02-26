#include <gtest/gtest.h>

#include <nx/vms/api/analytics/device_analytics_settings_data.h>

#include <nx/fusion/model_functions.h>

#include "analytics_settings_manager_test.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api::analytics; //< TODO: Remove lexical reduplication.

static const auto kEmptyReply = QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {},
        "settingsValues": {}
    }
)json");

static const auto kData1Reply = QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generateCars", "type": "CheckBox" } ]
            "type": "Settings"
        },
        "settingsValues": { "generateCars": true }
    }
)json");

static const auto kData2Reply= QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generatePersons", "type": "CheckBox" } ]
            "type": "Settings"
        },
        "settingsValues": { "generatePersons": false }
    }
)json");

/**
 * Validate VMS-17327: camera settings dialog did not get an update when a loaded model was empty.
 * Solution: add request status to a passed data, so dialog will know the status for sure.
 */
TEST_F(AnalyticsSettingsManagerTest, statusUpdateOnEmptyReply)
{
    const auto agentId = makeDeviceAgent();
    auto listener = manager()->getListener(agentId);
    ListenerNotifier notifier(listener);

    // Check manager made a request here.
    ASSERT_TRUE(m_serverInterfaceMock->requestWasSent(agentId));
    // Check listener is still waiting.
    ASSERT_TRUE(listener->data().status == DeviceAgentData::Status::loading);

    // Emulate network response.
    const auto reply = kEmptyReply;
    m_serverInterfaceMock->sendReply(agentId, reply);

    // Check listener sent actual data notification.
    ASSERT_EQ(notifier.counter, 1);
    ASSERT_TRUE(listener->data().status == DeviceAgentData::Status::ok);
}

TEST_F(AnalyticsSettingsManagerTest, listenToExternalChanges)
{
    const auto agentId = makeDeviceAgent();
    auto listener = manager()->getListener(agentId);
    ListenerNotifier notifier(listener);
    m_serverInterfaceMock->sendReply(agentId, kData1Reply);

    ASSERT_FALSE(m_serverInterfaceMock->requestWasSent(agentId));
    manager()->refreshSettings(agentId);
    ASSERT_TRUE(m_serverInterfaceMock->requestWasSent(agentId));

    // Emulate network response.
    m_serverInterfaceMock->sendReply(agentId, kData2Reply);

    // Check consumer received actual data.
    ASSERT_EQ(notifier.lastData.model, kData2Reply.settingsModel);
    ASSERT_EQ(notifier.lastData.values, kData2Reply.settingsValues);
}

} // namespace test
} // namespace nx::vms::client::desktop
