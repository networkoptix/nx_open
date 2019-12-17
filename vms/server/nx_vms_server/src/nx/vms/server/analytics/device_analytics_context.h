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
class StreamConverter;

class DeviceAnalyticsContext:
    public Connective<QObject>,
    public /*mixin*/ nx::vms::server::ServerModuleAware,
    public StreamDataReceptor
{
    Q_OBJECT

    using base_type = nx::vms::server::ServerModuleAware;
    using BindingMap = std::map<QnUuid, std::shared_ptr<DeviceAnalyticsBinding>>;
    using StreamProviderRequirementsMap =
        std::map<nx::vms::api::StreamIndex, StreamProviderRequirements>;

public:
    DeviceAnalyticsContext(
        QnMediaServerModule* severModule,
        const QnVirtualCameraResourcePtr& device);

    void setEnabledAnalyticsEngines(
        const resource::AnalyticsEngineResourceList& engines);
    void removeEngine(const resource::AnalyticsEngineResourcePtr& engine);

    void setMetadataSink(QWeakPointer<QnAbstractDataReceptor> metadataSink);
    void setSettings(const QString& engineId, const QJsonObject& settings);
    QJsonObject getSettings(const QString& engineId) const;

    // @return map Engine id -> has alive DeviceAgent
    std::map<QnUuid, bool> bindingStatuses() const;

    virtual void putData(const QnAbstractDataPacketPtr& data) override;
    virtual StreamProviderRequirements providerRequirements(
        nx::vms::api::StreamIndex streamIndex) const override;

    virtual void registerStream(nx::vms::api::StreamIndex streamIndex) override;

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
    void updateStreamProviderRequirements();

    // Collects and merges stream requirements from all the plugins.
    StreamProviderRequirementsMap getTotalProviderRequirements() const;

    // Calculates and sets motion analysis policy according to the binding requirements.
    StreamProviderRequirementsMap setUpMotionAnalysisPolicy(
        StreamProviderRequirementsMap requirements) const;

    // Adjusts stream provider requirements according to the registered stream count. E.g. if some
    // bindings prefer secondary stream data, but there is only the primary stream from the device,
    // all their requirements will still be served by the existing (primary) provider.
    StreamProviderRequirementsMap adjustProviderRequirementsByProviderCountUnsafe(
        StreamProviderRequirementsMap requirements) const;

    BindingMap analyticsBindingsSafe() const;
    std::shared_ptr<DeviceAnalyticsBinding> analyticsBindingUnsafe(const QnUuid& engineId) const;
    std::shared_ptr<DeviceAnalyticsBinding> analyticsBindingSafe(const QnUuid& engineId) const;

    bool isEngineAlreadyBoundUnsafe(const QnUuid& engineId) const;

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
    BindingMap m_bindings;
    QWeakPointer<QnAbstractDataReceptor> m_metadataSink;

    StreamProviderRequirementsMap m_cachedStreamProviderRequirements;

    std::atomic<Qn::ResourceStatus> m_previousDeviceStatus{Qn::ResourceStatus::NotDefined};
    nx::utils::FrequencyRestrictedCall<void, int, QnUuid> m_throwPluginEvent;
    std::map<QnUuid, int> m_skippedPacketCountByEngine;
    std::unique_ptr<StreamConverter> m_streamConverter;
    std::set<nx::vms::api::StreamIndex> m_registeredStreams;
};

} // namespace nx::vms::server::analytics
