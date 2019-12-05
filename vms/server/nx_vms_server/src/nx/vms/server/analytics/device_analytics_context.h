#pragma once

#include <map>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/frequency_restricted_call.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/abstract_video_data_receptor.h>
#include <nx/vms/server/sdk_support/file_utils.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/sdk/ptr.h>
#include <nx/sdk/i_string_map.h>

class QnAbstractDataReceptor;

namespace nx::vms::server::analytics {

class DeviceAnalyticsBinding;

class DeviceAnalyticsContext:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public AbstractVideoDataReceptor
{
    Q_OBJECT

    using base_type = nx::vms::server::ServerModuleAware;

public:
    DeviceAnalyticsContext(
        QnMediaServerModule* severModule,
        const QnVirtualCameraResourcePtr& device);

    void setEnabledAnalyticsEngines(
        const resource::AnalyticsEngineResourceList& engines);
    void addEngine(const resource::AnalyticsEngineResourcePtr& engine);
    void removeEngine(const resource::AnalyticsEngineResourcePtr& engine);

    void setMetadataSink(QWeakPointer<QnAbstractDataReceptor> metadataSink);
    void setSettings(const QString& engineId, const QVariantMap& settings);
    QVariantMap getSettings(const QString& engineId) const;

    virtual bool needsCompressedFrames() const override;
    virtual NeededUncompressedPixelFormats neededUncompressedPixelFormats() const override;
    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) override;

signals:
    void pluginDiagnosticEventTriggered(const nx::vms::event::PluginDiagnosticEventPtr&) const;

private:
    void at_deviceStatusChanged(const QnResourcePtr& resource);
    void at_deviceUpdated(const QnResourcePtr& resource);
    void at_devicePropertyChanged(const QnResourcePtr& resource, const QString& propertyName);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

private:
    void updateDevice(const QnResourcePtr& resource);
    void subscribeToDeviceChanges();
    void subscribeToRulesChanges();

    bool isDeviceAlive() const;
    void updateSupportedFrameTypes();

    std::shared_ptr<DeviceAnalyticsBinding> analyticsBinding(const QnUuid& engineId) const;

    bool isEngineAlreadyBound(const QnUuid& engineId) const;
    bool isEngineAlreadyBound(const resource::AnalyticsEngineResourcePtr& engine) const;

    QVariantMap prepareSettings(const QnUuid& engineId, const QVariantMap& settings);
    std::optional<QVariantMap> loadSettingsFromFile(
        const resource::AnalyticsEngineResourcePtr& engine) const;

    std::optional<QVariantMap> loadSettingsFromSpecificFile(
        const resource::AnalyticsEngineResourcePtr& engine,
        sdk_support::FilenameGenerationOptions filenameGenerationOptions) const;

    void reportSkippedFrames(int framesSkipped, QnUuid engineId) const;

private:
    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;
    std::map<QnUuid, std::shared_ptr<DeviceAnalyticsBinding>> m_bindings;
    QWeakPointer<QnAbstractDataReceptor> m_metadataSink;

    bool m_cachedNeedCompressedFrames{false};
    AbstractVideoDataReceptor::NeededUncompressedPixelFormats m_cachedUncompressedPixelFormats;
    bool m_missingUncompressedFrameWarningIssued = false;
    Qn::ResourceStatus m_previousDeviceStatus = Qn::ResourceStatus::NotDefined;
    nx::utils::FrequencyRestrictedCall<void, int, QnUuid> m_throwPluginEvent;
    std::map<QnUuid, int> m_skippedFrameCountByEngine;
};

} // namespace nx::vms::server::analytics
