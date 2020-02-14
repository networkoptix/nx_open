#pragma once

#include <memory>

#include <QtCore/QObject>
#include <QtCore/QJsonObject>

#include <api/server_rest_connection_fwd.h>

#include <core/resource/resource_fwd.h>

#include <nx/utils/uuid.h>

namespace nx::vms::api::analytics { struct DeviceAnalyticsSettingsResponse; }

namespace nx::vms::client::desktop {

class AnalyticsSettingsManager;
using AnalyticsSettingsCallback =
    rest::Callback<nx::vms::api::analytics::DeviceAnalyticsSettingsResponse>;

class NX_VMS_CLIENT_DESKTOP_API AnalyticsSettingsServerInterface
{
public:
    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) = 0;
    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) = 0;
};
using AnalyticsSettingsServerInterfacePtr = std::shared_ptr<AnalyticsSettingsServerInterface>;

struct DeviceAgentId
{
    QnUuid device;
    QnUuid engine;

    inline bool operator==(const DeviceAgentId& other) const
    {
        return device == other.device && engine == other.engine;
    }
};

uint qHash(const DeviceAgentId& key);

class NX_VMS_CLIENT_DESKTOP_API AnalyticsSettingsListener: public QObject
{
    Q_OBJECT

public:
    const DeviceAgentId agentId;

    QJsonObject model() const;
    QJsonObject values() const;

signals:
    void modelChanged(const QJsonObject& model);
    void valuesChanged(const QJsonObject& values);

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

    /**
     * Custom server interface can be passed to emulate behavior in unit tests. Ownership will be
     * shared. If not passed, default server rest interface will be created and used.
     */
    void setCustomServerInterface(AnalyticsSettingsServerInterfacePtr serverInterface);

    void refreshSettings(const DeviceAgentId& agentId);

    AnalyticsSettingsListenerPtr getListener(const DeviceAgentId& agentId);

    QJsonObject values(const DeviceAgentId& agentId) const;
    QJsonObject model(const DeviceAgentId& agentId) const;

    /**
     * Send changed actual values to the server.
     */
    Error applyChanges(const QHash<DeviceAgentId, QJsonObject>& valuesByAgentId);

    bool isApplyingChanges() const;

signals:
    void appliedChanges();

private:
    class Private;
    QScopedPointer<Private> d;

    friend class AnalyticsSettingsListener;
};

} // namespace nx::vms::client::desktop
