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
#include <nx/vms/server/analytics/types.h>
#include <nx/vms/server/analytics/settings.h>
#include <nx/vms/server/metrics/plugin_resource_binding_info_provider.h>
#include <nx/vms/server/sdk_support/types.h>

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
    public nx::vms::server::metrics::PluginResourceBindingInfoProvider
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

    virtual std::unique_ptr<metrics::PluginResourceBindingInfoHolder>
        bindingInfoHolder() const override;

    void registerMetadataSink(const QnResourcePtr& deviceResource, MetadataSinkPtr metadataSink);

    QWeakPointer<StreamDataReceptor> registerMediaSource(
        const QnUuid& deviceId,
        nx::vms::api::StreamIndex streamIndex);

    SettingsResponse setSettings(
        const QString& deviceId,
        const QString& engineId,
        const SetSettingsRequest& settings);

    SettingsResponse getSettings(
        const QString& deviceId, const QString& engineId) const;

    SettingsResponse setSettings(
        const QString& engineId, const SetSettingsRequest& settings);

    SettingsResponse getSettings(const QString& engineId) const;

public slots:
    void initExistingResources();

private:
    using AnalyticsEngineResourcePtr = nx::vms::server::resource::AnalyticsEngineResourcePtr;

    QSharedPointer<DeviceAnalyticsContext> deviceAnalyticsContextUnsafe(
        const QnUuid& deviceId) const;
    QSharedPointer<DeviceAnalyticsContext> deviceAnalyticsContextUnsafe(
        const resource::CameraPtr& device) const;

    void at_deviceAdded(const resource::CameraPtr& device);
    void at_deviceRemoved(const resource::CameraPtr& device);
    void at_deviceParentIdChanged(const resource::CameraPtr& device);

    void at_deviceUserEnabledAnalyticsEnginesChanged(const QnVirtualCameraResourcePtr& device);

    void at_deviceStatusChanged(const QnResourcePtr& deviceResource);

    void handleDeviceArrivalToServer(const resource::CameraPtr& device);
    void handleDeviceRemovalFromServer(const resource::CameraPtr& device);

    void at_engineAdded(const AnalyticsEngineResourcePtr& engine);
    void at_engineRemoved(const AnalyticsEngineResourcePtr& engine);
    void at_enginePropertyChanged(
        const AnalyticsEngineResourcePtr& engine,
        const QString& propertyName);

    void at_engineInitializationStateChanged(const AnalyticsEngineResourcePtr& engine);

    MetadataSinkSet metadataSinksUnsafe(const QnUuid& deviceId) const;

    QWeakPointer<ProxyStreamDataReceptor> mediaSourceUnsafe(const QnUuid& deviceId) const;

    nx::vms::server::resource::AnalyticsEngineResourceList localEngines() const;
    resource::CameraList localDevices() const;
    bool isLocalDevice(const resource::CameraPtr& device) const;

    QSet<QnUuid> compatibleEngineIds(const resource::CameraPtr& device) const;

    void updateCompatibilityWithEngines(const resource::CameraPtr& device);
    void updateCompatibilityWithDevices(const AnalyticsEngineResourcePtr& engine);
    void updateEnabledAnalyticsEngines(const resource::CameraPtr& device);

private:
    mutable nx::Mutex m_mutex;

    QThread* m_thread;

    std::map<QnUuid, QSharedPointer<DeviceAnalyticsContext>> m_deviceAnalyticsContexts;

    std::map<QnUuid, MetadataSinkSet> m_metadataSinks;

    std::map<QnUuid, QSharedPointer<ProxyStreamDataReceptor>> m_mediaSources;
};

} // namespace nx::vms::server::analytics
