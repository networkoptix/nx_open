#pragma once

#include <map>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>
#include <common/common_globals.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/frequency_restricted_call.h>

#include <nx/vms/event/events/events_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/analytics/i_stream_data_receptor.h>
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
    public IStreamDataReceptor
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
    void setSettings(const QString& engineId, const QJsonObject& settings);
    QJsonObject getSettings(const QString& engineId) const;

    // @return map Engine id -> has alive DeviceAgent
    std::map<QnUuid, bool> bindingStatuses() const;

    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual nx::vms::api::analytics::StreamTypes requiredStreamTypes() const override;

signals:
    void pluginDiagnosticEventTriggered(const nx::vms::event::PluginDiagnosticEventPtr&) const;

private:
    void at_deviceStatusChanged(const QnResourcePtr& resource);
    void at_deviceUpdated(const QnResourcePtr& resource);
    void at_devicePropertyChanged(const QnResourcePtr& resource, const QString& propertyName);
    void at_rulesUpdated(const QSet<QnUuid>& affectedResources);

private:
    void subscribeToDeviceChanges();
    void subscribeToRulesChanges();

    bool isDeviceAlive() const;
    void updateStreamRequirements();

    std::shared_ptr<DeviceAnalyticsBinding> analyticsBinding(const QnUuid& engineId) const;

    bool isEngineAlreadyBound(const QnUuid& engineId) const;

    QJsonObject prepareSettings(const QnUuid& engineId, const QJsonObject& settings);
    std::optional<QJsonObject> loadSettingsFromFile(
        const resource::AnalyticsEngineResourcePtr& engine) const;

    std::optional<QJsonObject> loadSettingsFromSpecificFile(
        const resource::AnalyticsEngineResourcePtr& engine,
        sdk_support::FilenameGenerationOptions filenameGenerationOptions) const;

    void reportSkippedFrames(int framesSkipped, QnUuid engineId) const;

private:
    mutable QnMutex m_mutex;
    QnVirtualCameraResourcePtr m_device;
    std::map<QnUuid, std::shared_ptr<DeviceAnalyticsBinding>> m_bindings;
    QWeakPointer<QnAbstractDataReceptor> m_metadataSink;

    nx::vms::api::analytics::StreamTypes m_cachedRequiredStreamTypes;

    bool m_missingUncompressedFrameWarningIssued = false;
    Qn::ResourceStatus m_previousDeviceStatus = Qn::ResourceStatus::NotDefined;
    nx::utils::FrequencyRestrictedCall<void, int, QnUuid> m_throwPluginEvent;
    std::map<QnUuid, int> m_skippedPacketCountByEngine;
};

} // namespace nx::vms::server::analytics
