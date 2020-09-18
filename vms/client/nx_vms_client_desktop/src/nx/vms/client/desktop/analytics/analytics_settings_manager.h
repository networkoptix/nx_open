#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <api/server_rest_connection_fwd.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>
#include <nx/utils/impl_ptr.h>

#include "analytics_settings_types.h"

namespace nx::vms::api::analytics { struct DeviceAgentSettingsResponse; }

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;
using AnalyticsSettingsCallback =
    rest::Callback<nx::vms::api::analytics::DeviceAgentSettingsResponse>;

class NX_VMS_CLIENT_DESKTOP_API AnalyticsSettingsServerInterface
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
};
using AnalyticsSettingsServerInterfacePtr = std::shared_ptr<AnalyticsSettingsServerInterface>;

class NX_VMS_CLIENT_DESKTOP_API AnalyticsSettingsListener: public QObject
{
    Q_OBJECT

public:
    const DeviceAgentId agentId;

    DeviceAgentData data() const;

signals:
    void dataChanged(const DeviceAgentData& data);

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
class NX_VMS_CLIENT_DESKTOP_API AnalyticsSettingsManager: public QObject
{
    Q_OBJECT

public:
    enum class Error
    {
        noError,
        busy,
    };

    AnalyticsSettingsManager(QObject* parent = nullptr);
    virtual ~AnalyticsSettingsManager() override;

    void setResourcePool(QnResourcePool* resourcePool);

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

    AnalyticsSettingsListenerPtr getListener(const DeviceAgentId& agentId);

    DeviceAgentData data(const DeviceAgentId& agentId) const;

    /**
     * Send changed actual values to the server.
     */
    Error applyChanges(const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId);

private:
    class Private;
    nx::utils::ImplPtr<Private> d;

    friend class AnalyticsSettingsListener;
};

} // namespace nx::vms::client::desktop
