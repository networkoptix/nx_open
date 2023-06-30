// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <deque>

#include <gtest/gtest.h>

#include <nx/vms/client/core/analytics/analytics_settings_manager.h>
#include <nx/vms/client/desktop/test_support/test_context.h>
#include <nx/vms/common/test_support/resource/camera_resource_stub.h>

namespace nx::vms::client::desktop {
namespace test {

class AnalyticsSettingsMockApiInterface: public core::AnalyticsSettingsServerInterface
{
public:
    struct RequestInfo
    {
        enum class Type { get, apply };
        Type type = Type::get;
        rest::Handle handle = {};
        core::DeviceAgentId agentId;
        core::AnalyticsSettingsCallback callback;
    };

    AnalyticsSettingsMockApiInterface();

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        core::AnalyticsSettingsCallback callback) override;

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        const QnUuid& settingsModelId,
        core::AnalyticsSettingsCallback callback) override;

    virtual rest::Handle activeSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        core::AnalyticsActiveSettingsCallback callback) override;

    bool requestWasSent(const core::DeviceAgentId& agentId) const;

    void sendReply(
        const core::DeviceAgentId& agentId,
        const nx::vms::api::analytics::DeviceAgentSettingsResponse& response,
        bool success = true);

private:
    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        core::AnalyticsSettingsCallback callback);

    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        core::AnalyticsActiveSettingsCallback callback);

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
    ListenerNotifier(const core::AnalyticsSettingsListenerPtr& listener);

    core::DeviceAgentData lastData;
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

    core::AnalyticsSettingsManager* manager() const;

    QnResourcePool* resourcePool() const;

    QScopedPointer<core::AnalyticsSettingsManager> m_manager;
    AnalyticsSettingsMockApiInterfacePtr m_serverInterfaceMock;
};

} // namespace test
} // namespace nx::vms::client::desktop
