#pragma once

#include <map>

#include <utils/common/connective.h>
#include <core/resource/resource_fwd.h>

#include <nx/mediaserver/server_module_aware.h>
#include <nx/mediaserver/sdk_support/pointers.h>
#include <nx/mediaserver/analytics/abstract_video_data_receptor.h>
#include <nx/sdk/analytics/device_agent.h>

class QnAbstractDataReceptor;

namespace nx::mediaserver::analytics {

class DeviceAnalyticsBinding;

class DeviceAnalyticsContext:
    public Connective<QObject>,
    public nx::mediaserver::ServerModuleAware,
    public AbstractVideoDataReceptor
{
    using base_type = nx::mediaserver::ServerModuleAware;
public:
    DeviceAnalyticsContext(
        QnMediaServerModule* severModule,
        const QnVirtualCameraResourcePtr& device);

    void setEnabledAnalyticsEngines(
        const nx::vms::common::AnalyticsEngineResourceList& engines);
    void addEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine);
    void removeEngine(const nx::vms::common::AnalyticsEngineResourcePtr& engine);

    void setMetadataSink(QWeakPointer<QnAbstractDataReceptor> metadataSink);

    virtual bool needsCompressedFrames() const override;
    virtual NeededUncompressedPixelFormats neededUncompressedPixelFormats() const override;
    virtual void putFrame(
        const QnConstCompressedVideoDataPtr& compressedFrame,
        const CLConstVideoDecoderOutputPtr& uncompressedFrame) override;

private:
    void at_deviceUpdated(const QnResourcePtr& device);
    void at_devicePropertyChanged(const QnResourcePtr& device, const QString& propertyName);

private:
    void subscribeToDeviceChanges();
    bool isDeviceAlive() const;
    void updateSupportedFrameTypes();

    bool isEngineAlreadyBound(const QnUuid& engineId) const;
    bool isEngineAlreadyBound(const nx::vms::common::AnalyticsEngineResourcePtr& engine) const;

    void issueMissingUncompressedFrameWarningOnce();

private:
    QnVirtualCameraResourcePtr m_device;
    std::map<QnUuid, std::shared_ptr<DeviceAnalyticsBinding>> m_bindings;
    QWeakPointer<QnAbstractDataReceptor> m_metadataSink;

    bool m_cachedNeedCompressedFrames{false};
    AbstractVideoDataReceptor::NeededUncompressedPixelFormats m_cachedUncompressedPixelFormats;
    bool m_uncompressedFrameWarningIssued{false};
};

} // namespace nx::mediaserver::analytics
