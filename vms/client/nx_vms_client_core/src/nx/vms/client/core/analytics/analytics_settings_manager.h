// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <QtCore/QJsonObject>
#include <QtCore/QObject>

#include <api/model/analytics_actions.h>
#include <api/server_rest_connection_fwd.h>
#include <core/resource/resource_fwd.h>
#include <nx/utils/impl_ptr.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/core/system_context_aware.h>
#include "analytics_settings_types.h"

class QnCommonMessageProcessor;

namespace nx::vms::api::analytics {

struct DeviceAgentSettingsResponse;
struct DeviceAgentActiveSettingChangedResponse;

} // namespace nx::vms::api::analytics

namespace nx::vms::client::core {

class AnalyticsSettingsManager;

using AnalyticsSettingsCallback =
    rest::Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>;

using AnalyticsActiveSettingsCallback =
    rest::Callback<nx::vms::api::analytics::DeviceAgentActiveSettingChangedResponse>;

using AnalyticsSettingsActionCallback = rest::Callback<AnalyticsActionResult>;

class NX_VMS_CLIENT_CORE_API AnalyticsSettingsServerInterface
{
public:
    virtual ~AnalyticsSettingsServerInterface();

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) = 0;
    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        const QnUuid& settingsModelId,
        AnalyticsSettingsCallback callback) = 0;
    virtual rest::Handle activeSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        AnalyticsActiveSettingsCallback callback) = 0;
};
using AnalyticsSettingsServerInterfacePtr = std::shared_ptr<AnalyticsSettingsServerInterface>;

class NX_VMS_CLIENT_CORE_API AnalyticsSettingsListener: public QObject
{
    Q_OBJECT

public:
    const DeviceAgentId agentId;

    DeviceAgentData data() const;

signals:
    void dataChanged(const DeviceAgentData& data);
    void previewDataReceived(const DeviceAgentData& data);
    void actionResultReceived(const AnalyticsActionResult& result);

protected:
    AnalyticsSettingsListener(const DeviceAgentId& agentId, AnalyticsSettingsManager* manager);

private:
    AnalyticsSettingsManager* m_manager = nullptr;

    friend class AnalyticsSettingsManager;
};

using AnalyticsSettingsListenerPtr = std::shared_ptr<AnalyticsSettingsListener>;

/**
 * Centralized storage class, which ensures all subscribers ('listeners') are always have an access
 * to an actual version of the analytics settings. When some class changes settings, manager sends
 * these changes to the server and notifies other listeners about it. When changes are received from
 * the server (using special transaction), actual values are re-requested.
 */
class NX_VMS_CLIENT_CORE_API AnalyticsSettingsManager: public QObject, public SystemContextAware
{
    Q_OBJECT

public:
    enum class Error
    {
        noError,
        busy,
    };

    AnalyticsSettingsManager(SystemContext* systemContext, QObject* parent = nullptr);
    virtual ~AnalyticsSettingsManager() override;

    /**
     * Server interface should be passed to send actual requests to the media server. Ownership will
     * be shared.
     */
    void setServerInterface(AnalyticsSettingsServerInterfacePtr serverInterface);

    void refreshSettings(const DeviceAgentId& agentId);

    /**
     * Update stored Settings Model and values with the provided data.
     */
    void storeSettings(
        const DeviceAgentId& agentId,
        const nx::vms::api::analytics::DeviceAgentSettingsResponse& data);

    bool activeSettingsChanged(
        const DeviceAgentId& agentId,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues);

    AnalyticsSettingsListenerPtr getListener(const DeviceAgentId& agentId);

    DeviceAgentData data(const DeviceAgentId& agentId) const;

    /**
     * Send changed actual values to the server.
     */
    Error applyChanges(const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId);

private:
    struct Private;
    nx::utils::ImplPtr<Private> d;

    friend class AnalyticsSettingsListener;
};

} // namespace nx::vms::client::core
