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
#include <nx/vms/server/analytics/abstract_video_data_receptor.h>
#include <core/dataconsumer/data_copier.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/server/resource/resource_fwd.h>
#include <utils/common/value_cache.h>

static const int META_FRAME_INTERVAL = 10;
static const int META_DATA_DURATION_MS = 300;
static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 1024 * 768;

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

    virtual bool needMetadata();

    void onStreamReopen();

    virtual void onGotVideoFrame(
        const QnCompressedVideoDataPtr& videoData,
        const QnLiveStreamParams& currentLiveParams,
        bool isCameraControlRequired);

    virtual void onGotAudioFrame(const QnCompressedAudioDataPtr& audioData);

    virtual void updateSoftwareMotion();

    static bool hasRunningLiveProvider(QnNetworkResource* netRes);
    virtual void startIfNotRunning() override;

    virtual bool isCameraControlDisabled() const;
    void filterMotionByMask(const QnMetaDataV1Ptr& motion);

    void setOwner(QnSharedResourcePointer<QnAbstractVideoCamera> owner);
    virtual QnSharedResourcePointer<QnAbstractVideoCamera> getOwner() const override;
    virtual void pleaseReopenStream() = 0;

    virtual QnConstResourceAudioLayoutPtr getDPAudioLayout() const;
    void setDoNotConfigureCamera(bool value) { m_doNotConfigureCamera = value; }
protected:
    QnAbstractCompressedMetadataPtr getMetadata();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    virtual void onStreamResolutionChanged(int channelNumber, const QSize& picSize);
    bool needHardwareMotion();
protected:
    mutable QnMutex m_liveMutex;

private:
    float getDefaultFps() const;

    bool needAnalyzeMotion();

    void updateStreamResolution(int channelNumber, const QSize& newResolution);

    void saveMediaStreamParamsIfNeeded(const QnCompressedVideoDataPtr& videoData);

    void saveBitrateIfNeeded(
        const QnCompressedVideoDataPtr& videoData,
        const QnLiveStreamParams& liveParams,
        bool isCameraConfigured);

    void emitAnalyticsEventIfNeeded(const QnAbstractCompressedMetadataPtr& metadata);
    QnLiveStreamParams mergeWithAdvancedParams(const QnLiveStreamParams& params);

    nx::vms::server::analytics::AbstractVideoDataReceptorPtr
        getVideoDataReceptorForMetadataPluginsIfNeeded(
            const QnCompressedVideoDataPtr& compressedFrame,
            bool* outNeedUncompressedFrame);

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

    QnMotionEstimation m_motionEstimation[CL_MAX_CHANNELS];

    QSize m_videoResolutionByChannelNumber[CL_MAX_CHANNELS];
    int m_softMotionLastChannel;
    std::atomic<int> m_videoChannels;
    QnVirtualCameraResourcePtr m_cameraRes;
    simd128i *m_motionMaskBinData[CL_MAX_CHANNELS];
    QElapsedTimer m_resolutionCheckTimer;
    int m_framesSincePrevMediaStreamCheck;
    QWeakPointer<QnAbstractVideoCamera> m_owner;

    QWeakPointer<nx::vms::server::analytics::AbstractVideoDataReceptor> m_videoDataReceptor;
    QSharedPointer<MetadataDataReceptor> m_metadataReceptor;
    QnAbstractDataReceptorPtr m_analyticsEventsSaver;
    QSharedPointer<DataCopier> m_dataReceptorMultiplexer;
    bool m_doNotConfigureCamera = false;
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif // defined(ENABLE_DATA_PROVIDERS)
