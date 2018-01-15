#pragma once

#ifdef ENABLE_DATA_PROVIDERS

#include <atomic>
#include <map>
#include <queue>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include <motion/motion_estimation.h>

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

#include <nx/utils/safe_direct_connection.h>

#include <core/resource/resource_fwd.h>
#include <core/resource/resource_media_layout.h>

#include <analytics/common/video_metadata_plugin.h>
#include <core/dataprovider/live_stream_params.h>

static const int  META_DATA_DURATION_MS = 300;
static const int MIN_SECOND_STREAM_FPS = 2;
static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 1024 * 768;

namespace nx {
namespace analytics {

class NaiveDetectionSmoother;

} // namespace analytics
} // namespace nx

class QnLiveStreamProvider;

class QnLiveStreamProvider: public QnAbstractMediaStreamDataProvider
{
public:
    QnLiveStreamProvider(const QnResourcePtr& res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(Qn::ConnectionRole role) override;
    Qn::ConnectionRole getRole() const;
    int encoderIndex() const;

    void setParams(const QnLiveStreamParams& params);
    //void setSecondaryQuality(Qn::SecondStreamQuality  quality);
    //virtual void setQuality(Qn::StreamQuality q);
    //virtual void setFps(float f);
    virtual void setCameraControlDisabled(bool value);

    // for live providers only
    bool isMaxFps() const;

    void onPrimaryParamsChanged(const QnLiveStreamParams& params);
    QnLiveStreamParams getLiveParams();

    bool needMetaData();

    void onStreamReopen();

    virtual void onGotVideoFrame(
        const QnCompressedVideoDataPtr& videoData,
        const QnLiveStreamParams& currentLiveParams,
        bool isCameraControlRequired);

    virtual void onGotAudioFrame(const QnCompressedAudioDataPtr& audioData);

    virtual void updateSoftwareMotion();
    bool canChangeStatus() const;

    static bool hasRunningLiveProvider(QnNetworkResource* netRes);
    virtual void startIfNotRunning() override;

    bool isCameraControlDisabled() const;
    void filterMotionByMask(const QnMetaDataV1Ptr& motion);
    void updateSoftwareMotionStreamNum();

    void setOwner(QnSharedResourcePointer<QnAbstractVideoCamera> owner);
    virtual QnSharedResourcePointer<QnAbstractVideoCamera> getOwner() const override;
    virtual void pleaseReopenStream() = 0;

protected:
    QnAbstractCompressedMetadataPtr getMetaData();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    virtual Qn::ConnectionRole roleForMotionEstimation();
    virtual void onStreamResolutionChanged( int channelNumber, const QSize& picSize );

protected:
    mutable QnMutex m_livemutex;

private:
    float getDefaultFps() const;
    bool needAnalyzeStream(Qn::ConnectionRole role);

    void updateStreamResolution(int channelNumber, const QSize& newResolution);
    void extractMediaStreamParams(
        const QnCompressedVideoDataPtr& videoData,
        QSize* const newResolution,
        std::map<QString, QString>* const customStreamParams = nullptr);

    void saveMediaStreamParamsIfNeeded(const QnCompressedVideoDataPtr& videoData);
    void saveBitrateIfNeeded(
        const QnCompressedVideoDataPtr& videoData,
        const QnLiveStreamParams& liveParams,
        bool isCameraConfigured);

    void emitAnalyticsEventIfNeeded(const QnAbstractCompressedMetadataPtr& metadata);

private:
    // NOTE: m_newLiveParams are going to update a little before the actual stream gets reopened
    // TODO: find out the way to keep it in sync besides pleaseReopenStream() call (which causes delay)
    QnLiveStreamParams m_newLiveParams;

    bool m_prevCameraControlDisabled;
    unsigned int m_framesSinceLastMetaData;
    QTime m_timeSinceLastMetaData;
    size_t m_totalVideoFrames;
    size_t m_totalAudioFrames;

    QnMutex m_motionRoleMtx;
    Qn::ConnectionRole m_softMotionRole;
    QString m_forcedMotionStream;

#ifdef ENABLE_SOFTWARE_MOTION_DETECTION
    QnMotionEstimation m_motionEstimation[CL_MAX_CHANNELS];
#endif

    std::unique_ptr<nx::analytics::VideoMetadataPlugin> m_videoMetadataPlugin;
    std::unique_ptr<nx::analytics::NaiveDetectionSmoother> m_detectionSmoother;

    QSize m_videoResolutionByChannelNumber[CL_MAX_CHANNELS];
    int m_softMotionLastChannel;
    std::atomic<int> m_videoChannels;
    QnVirtualCameraResourcePtr m_cameraRes;
    simd128i *m_motionMaskBinData[CL_MAX_CHANNELS];
    QElapsedTimer m_resolutionCheckTimer;
    int m_framesSincePrevMediaStreamCheck;
    QWeakPointer<QnAbstractVideoCamera> m_owner;
    std::queue<QnAbstractCompressedMetadataPtr> m_metadataQueue;
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif // ENABLE_DATA_PROVIDERS
