#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>

#include <test_support/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop {
namespace test {

class AnalyticsSettingsMockApiInterface: public AnalyticsSettingsServerInterface
{
public:
    struct RequestInfo
    {
        enum class Type { get, apply };
        Type type = Type::get;
        rest::Handle handle = {};
        DeviceAgentId agentId;
        AnalyticsSettingsCallback callback;
    };

    AnalyticsSettingsMockApiInterface();

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override;

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) override;

    bool requestWasSent(const DeviceAgentId& agentId) const;

    void sendReply(
        const DeviceAgentId& agentId,
        const nx::vms::api::analytics::DeviceAnalyticsSettingsResponse& response,
        bool success = true);

private:
    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback);

    rest::Handle m_lastHandle = 0;
    std::deque<RequestInfo> m_requests;
};
using AnalyticsSettingsMockApiInterfacePtr = std::shared_ptr<AnalyticsSettingsMockApiInterface>;

/**
 * Class for convinient checks if listener dataChanged signal was emitted.
 */
class ListenerNotifier: public QObject
{
public:
    ListenerNotifier(const AnalyticsSettingsListenerPtr& listener);

    DeviceAgentData lastData;
    int counter = 0;
};

class AnalyticsSettingsManagerTest: public ::testing::Test
{
public:
    AnalyticsSettingsManagerTest();
    virtual ~AnalyticsSettingsManagerTest() override;

protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp();

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown();

    nx::CameraResourceStubPtr addCamera();

    nx::vms::common::AnalyticsEngineResourcePtr addEngine();

    DeviceAgentId makeDeviceAgent();

    AnalyticsSettingsManager* manager() const;

    QnResourcePool* resourcePool() const;

    // Declares the variables your tests want to use.
    struct Environment;
    QScopedPointer<Environment> m_environment;
    QScopedPointer<AnalyticsSettingsManager> m_manager;
    AnalyticsSettingsMockApiInterfacePtr m_serverInterfaceMock;
};

} // namespace test
} // namespace nx::vms::client::desktop
