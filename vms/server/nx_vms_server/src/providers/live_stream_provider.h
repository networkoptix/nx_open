#pragma once
#if defined(ENABLE_DATA_PROVIDERS)

#include <atomic>
#include <map>

#include <QtCore/QElapsedTimer>

#include <motion/motion_estimation.h>

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

#include <core/resource/camera_resource.h>
#include <core/resource/resource_fwd.h>
#include <core/dataprovider/live_stream_params.h>
#include <nx/vms/server/analytics/stream_data_receptor.h>
#include <core/dataconsumer/data_copier.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <nx/utils/value_cache.h>

#include <nx/analytics/metadata_logger.h>

static const int META_FRAME_INTERVAL = 10;
static const int META_DATA_DURATION_MS = 300;
static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 1024 * 768;

namespace nx::streaming { class InStreamCompressedMetadata; }

class QnLiveStreamProvider:
    public QnAbstractMediaStreamDataProvider,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
public:
    class MetadataDataReceptor;

    QnLiveStreamProvider(const nx::vms::server::resource::CameraPtr& res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(Qn::ConnectionRole role) override;
    nx::vms::api::StreamIndex encoderIndex() const;

    void setPrimaryStreamParams(const QnLiveStreamParams& params);

    virtual void setCameraControlDisabled(bool value);

    // For live providers only.
    bool isMaxFps() const;

    void onPrimaryFpsChanged(int primaryFps);
    QnLiveStreamParams getLiveParams();
    QnLiveStreamParams getActualParams() const;

    virtual bool needMetadata();

    void onStreamReopen();

    virtual void onGotVideoFrame(
        const QnCompressedVideoDataPtr& videoData,
        bool isCameraControlRequired);

    virtual void onGotAudioFrame(const QnCompressedAudioDataPtr& audioData);

    virtual void onGotInStreamMetadata(
        const std::shared_ptr<nx::streaming::InStreamCompressedMetadata>& metadata);

    virtual void updateSoftwareMotion();

    static bool hasRunningLiveProvider(QnNetworkResource* netRes);
    virtual void startIfNotRunning() override;
    virtual void start(Priority priority = InheritPriority) override;
    void disableStartThread();

    virtual bool isCameraControlDisabled() const;
    void filterMotionByMask(const QnMetaDataV1Ptr& motion);

    void setOwner(QnSharedResourcePointer<QnAbstractVideoCamera> owner);
    virtual QnSharedResourcePointer<QnAbstractVideoCamera> getOwner() const override;
    virtual void pleaseReopenStream() = 0;

    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    void setDoNotConfigureCamera(bool value) { m_doNotConfigureCamera = value; }
    virtual void pleaseStop() override;
protected:
    QnAbstractCompressedMetadataPtr getMetadata();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    virtual void onStreamResolutionChanged(int channelNumber, const QSize& picSize);
    bool needHardwareMotion();
    virtual void afterRun() override;

    virtual void processMetadata(const QnCompressedVideoDataPtr& compressedFrame);
protected:
    mutable QnMutex m_liveMutex;

private:
    float getDefaultFps() const;

    bool doesStreamSuitMotionAnalysisRequirements();
    void strictFpsToLimit(float* fps) const;

    bool doesFrameSuitMotionAnalysisRequirements(
        const QnCompressedVideoDataPtr& compressedFrame) const;

    void checkAndUpdatePrimaryStreamResolution(const QnCompressedVideoDataPtr& compressedFrame);
    void updateStreamResolution(int channelNumber, const QSize& newResolution);

    void saveMediaStreamParamsIfNeeded(const QnCompressedVideoDataPtr& videoData);

    void saveBitrateIfNeeded(
        const QnCompressedVideoDataPtr& videoData,
        const QnLiveStreamParams& liveParams,
        bool isCameraConfigured);

    QnLiveStreamParams mergeWithAdvancedParams(const QnLiveStreamParams& params);
    void startUnsafe(Priority priority = InheritPriority);
private:
    // NOTE: m_newLiveParams are going to update a little before the actual stream gets reopened.
    // TODO: Find the way to keep it in sync besides pleaseReopenStream() call, which causes delay.
    QnLiveStreamParams m_liveParams;
    int m_primaryFps = 0;

    bool m_prevCameraControlDisabled;
    unsigned int m_framesSinceLastMetaData;
    QTime m_timeSinceLastMetaData;
    size_t m_totalVideoFrames;
    size_t m_totalAudioFrames;

    std::vector<std::unique_ptr<QnMotionEstimation>> m_motionEstimation;

    QSize m_videoResolutionByChannelNumber[CL_MAX_CHANNELS];
    int m_softMotionLastChannel;
    std::atomic<int> m_videoChannels;
    QnVirtualCameraResourcePtr m_cameraRes;
    simd128i *m_motionMaskBinData[CL_MAX_CHANNELS];
    QElapsedTimer m_resolutionCheckTimer;
    std::atomic<int> m_framesSincePrevMediaStreamCheck;
    QWeakPointer<QnAbstractVideoCamera> m_owner;

    QWeakPointer<nx::vms::server::analytics::StreamDataReceptor> m_streamDataReceptor;
    QSharedPointer<MetadataDataReceptor> m_metadataReceptor;
    QnAbstractDataReceptorPtr m_analyticsEventsSaver;
    QSharedPointer<DataCopier> m_dataReceptorMultiplexer;
    bool m_doNotConfigureCamera = false;

    std::unique_ptr<nx::analytics::MetadataLogger> m_metadataLogger;
    std::atomic_bool m_canStartThread{true};
    QnMetaDataV1Ptr m_lastMotionMetadata;
    bool m_restartRequested;
    QnMutex m_startMutex;
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif // defined(ENABLE_DATA_PROVIDERS)
