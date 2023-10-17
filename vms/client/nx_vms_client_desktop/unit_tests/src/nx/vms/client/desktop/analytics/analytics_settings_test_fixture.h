// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

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
        const QnUuid& settingsModelId,
        AnalyticsSettingsCallback callback) override;

    virtual rest::Handle activeSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        AnalyticsActiveSettingsCallback callback) override;

    bool requestWasSent(const DeviceAgentId& agentId) const;

    void sendReply(
        const DeviceAgentId& agentId,
        const nx::vms::api::analytics::DeviceAgentSettingsResponse& response,
        bool success = true);

private:
    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback);

    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsActiveSettingsCallback callback);

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

class AnalyticsSettingsTestFixture: public ContextBasedTest
{
protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override;

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override;

    nx::vms::common::AnalyticsEngineResourcePtr addEngine();

    AnalyticsSettingsManager* manager() const;

    QnResourcePool* resourcePool() const;

    QScopedPointer<AnalyticsSettingsManager> m_manager;
    AnalyticsSettingsMockApiInterfacePtr m_serverInterfaceMock;
};

} // namespace test
} // namespace nx::vms::client::desktop
