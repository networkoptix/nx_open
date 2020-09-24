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

    void whenDataReceived(const DeviceAgentSettingsResponse& data)
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

} // namespace test
} // namespace nx::vms::client::desktop
