#pragma once

#include <map>
#include <memory>

#include <QtCore/QObject>
#include <QtCore/QThread>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <core/dataconsumer/abstract_data_receptor.h>

#include <nx/vms/api/data/analytics_data.h>

#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/rule_holder.h>
#include <nx/vms/server/metrics/i_plugin_metrics_provider.h>

#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/json.h>

class QnMediaServerModule;
class QnCompressedVideoData;

namespace nx::vms::server::analytics {

class DeviceAnalyticsContext;
class ProxyStreamDataReceptor;
class StreamDataReceptor;

class Manager final:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public nx::vms::server::metrics::IPluginMetricsProvider
{
    Q_OBJECT

public:
    Manager(QnMediaServerModule* serverModule);
    ~Manager();

    void init();
    void stop();
    void at_resourceAdded(const QnResourcePtr& resource);
    void at_resourceRemoved(const QnResourcePtr& resource);

    void at_resourceParentIdChanged(const QnResourcePtr& resource);
    void at_resourcePropertyChanged(const QnResourcePtr& resource, const QString& propertyName);

    virtual std::vector<nx::vms::server::metrics::PluginMetrics> metrics() const override;

    void registerMetadataSink(
        const QnResourcePtr& deviceResource,
        QWeakPointer<QnAbstractDataReceptor> metadataSink);

    QWeakPointer<StreamDataReceptor> registerMediaSource(
        const QnUuid& deviceId,
        nx::vms::api::StreamIndex streamIndex);

    void setSettings(const QString& deviceId,
        const QString& engineId,
        const QJsonObject& deviceAgentSettings);

    QJsonObject getSettings(const QString& deviceId, const QString& engineId) const;

    void setSettings(const QString& engineId, const QJsonObject& engineSettings);
    QJsonObject getSettings(const QString& engineId) const;

public slots:
    void initExistingResources();

private:
    using AnalyticsEngineResourcePtr = nx::vms::server::resource::AnalyticsEngineResourcePtr;

    QSharedPointer<DeviceAnalyticsContext> deviceAnalyticsContextUnsafe(
        const QnUuid& deviceId) const;
    QSharedPointer<DeviceAnalyticsContext> deviceAnalyticsContextUnsafe(
        const QnVirtualCameraResourcePtr& device) const;

    void at_deviceAdded(const QnVirtualCameraResourcePtr& device);
    void at_deviceRemoved(const QnVirtualCameraResourcePtr& device);
    void at_deviceParentIdChanged(const QnVirtualCameraResourcePtr& device);

    void at_deviceUserEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& device);

    void at_deviceStatusChanged(const QnResourcePtr& deviceResource);

    void handleDeviceArrivalToServer(const QnVirtualCameraResourcePtr& device);
    void handleDeviceRemovalFromServer(const QnVirtualCameraResourcePtr& device);

    void at_engineAdded(const AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(
        const AnalyticsEngineResourcePtr& engine,
        const QString& propertyName);

    void at_engineInitializationStateChanged(const AnalyticsEngineResourcePtr& engine);

    QWeakPointer<QnAbstractDataReceptor> metadataSinkUnsafe(const QnUuid& deviceId) const;
    QWeakPointer<ProxyStreamDataReceptor> mediaSourceUnsafe(const QnUuid& deviceId) const;

    nx::vms::server::resource::AnalyticsEngineResourceList localEngines() const;
    QnVirtualCameraResourceList localDevices() const;
    bool isLocalDevice(const QnVirtualCameraResourcePtr& device) const;

    QSet<QnUuid> compatibleEngineIds(const QnVirtualCameraResourcePtr& device) const;

    void updateCompatibilityWithEngines(const QnVirtualCameraResourcePtr& device);
    void updateCompatibilityWithDevices(const AnalyticsEngineResourcePtr& engine);
    void updateEnabledAnalyticsEngines(const QnVirtualCameraResourcePtr& device);

private:
    mutable nx::utils::Mutex m_mutex;

    QThread* m_thread;

    std::map<QnUuid, QSharedPointer<DeviceAnalyticsContext>> m_deviceAnalyticsContexts;

    // TODO: Switch to std pointers.
    std::map<QnUuid, QWeakPointer<QnAbstractDataReceptor>> m_metadataSinks;
    std::map<QnUuid, QSharedPointer<ProxyStreamDataReceptor>> m_mediaSources;
};

} // namespace nx::vms::server::analytics
