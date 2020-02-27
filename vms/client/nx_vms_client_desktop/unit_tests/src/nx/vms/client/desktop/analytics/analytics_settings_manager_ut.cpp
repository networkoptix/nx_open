#include <gtest/gtest.h>

#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/api/analytics/device_analytics_settings_data.h>

#include <nx/fusion/model_functions.h>

#include "analytics_settings_test_fixture.h"

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api::analytics; //< TODO: Remove lexical reduplication.
using namespace nx::vms::common;

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

    DeviceAgentId deviceAgentId() const
    {
        return *m_deviceAgentId;
    }

    AnalyticsSettingsListenerPtr listener() const
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
        m_deviceAgentId.reset(new DeviceAgentId{camera->getId(), engine->getId()});
        m_listener = manager()->getListener(deviceAgentId());
        m_notifier.reset(new ListenerNotifier(listener()));
    }

    void whenDataReceived(const DeviceAnalyticsSettingsResponse& data)
    {
        m_serverInterfaceMock->sendReply(deviceAgentId(), data);
    }

    bool requestWasSent() const
    {
        return m_serverInterfaceMock->requestWasSent(deviceAgentId());
    }

private:
    QScopedPointer<DeviceAgentId> m_deviceAgentId;
    AnalyticsSettingsListenerPtr m_listener;
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
    ASSERT_TRUE(listener()->data().status == DeviceAgentData::Status::loading);

    // Emulate network response.
    whenDataReceived(kEmptyReply);

    // Check listener sent actual data notification.
    ASSERT_EQ(notifier()->counter, 1);
    ASSERT_TRUE(listener()->data().status == DeviceAgentData::Status::ok);
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
    ASSERT_EQ(notifier()->lastData.model, kData2Reply.settingsModel);
    ASSERT_EQ(notifier()->lastData.values, kData2Reply.settingsValues);
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
    ASSERT_EQ(listener()->data().values, actualValues);
}

} // namespace test
} // namespace nx::vms::client::desktop
