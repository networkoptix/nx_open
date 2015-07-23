#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#ifdef ENABLE_DATA_PROVIDERS

#include <map>

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include "motion/motion_estimation.h"
#include "core/datapacket/audio_data_packet.h"
#include "core/datapacket/video_data_packet.h"
#include "core/resource/resource_fwd.h"
#include "media_streamdataprovider.h"
#include <core/resource/resource_media_layout.h>


static const int  META_DATA_DURATION_MS = 300;
static const int MIN_SECOND_STREAM_FPS = 2;
static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 720 * 576;
//#define DESIRED_SECOND_STREAM_FPS (7)
//#define MIN_SECOND_STREAM_FPS (2)

class QnLiveStreamProvider;

class QnAbstractVideoCamera
{
public:
    virtual QSharedPointer<QnLiveStreamProvider> getPrimaryReader() = 0;
    virtual QSharedPointer<QnLiveStreamProvider> getSecondaryReader() = 0;
};

struct QnLiveStreamParams
{
    Qn::StreamQuality quality;
    Qn::SecondStreamQuality secondaryQuality;
    mutable float fps;

    QnLiveStreamParams();
    bool operator ==(const QnLiveStreamParams& rhs);
    bool operator !=(const QnLiveStreamParams& rhs);
};

class QnLiveStreamProvider: public QnAbstractMediaStreamDataProvider
{
public:
    QnLiveStreamProvider(const QnResourcePtr& res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(Qn::ConnectionRole role) override;
    Qn::ConnectionRole getRole() const;
    int encoderIndex() const;

    void setSecondaryQuality(Qn::SecondStreamQuality  quality);
    virtual void setQuality(Qn::StreamQuality q);
    virtual void setCameraControlDisabled(bool value);

    // for live providers only 
    virtual void setFps(float f);
    bool isMaxFps() const;

    void onPrimaryFpsUpdated(int newFps);
    QnLiveStreamParams getLiveParams();

    // I assume this function is called once per video frame 
    bool needMetaData(); 

    virtual void onGotVideoFrame(const QnCompressedVideoDataPtr& videoData,
                                 const QnLiveStreamParams& currentLiveParams,
                                 bool isCameraControlRequired);

    virtual void onGotAudioFrame(const QnCompressedAudioDataPtr& audioData);

    void setUseSoftwareMotion(bool value);

    virtual void updateSoftwareMotion();
    bool canChangeStatus() const;

    virtual bool secondaryResolutionIsLarge() const { return false; }

    static bool hasRunningLiveProvider(QnNetworkResource* netRes);

    /*!
        Start provider if not running yet.
        @param canTouchCameraSettings can control camera settings if true
    */
    void startIfNotRunning();

    bool isCameraControlDisabled() const;
    void filterMotionByMask(const QnMetaDataV1Ptr& motion);
    void updateSoftwareMotionStreamNum();

    void setOwner(QnAbstractVideoCamera* owner);
    virtual void pleaseReopenStream() = 0;
protected:
    /*! Called when @param currentStreamParams are updated */

    QnMetaDataV1Ptr getMetaData();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    virtual Qn::ConnectionRole roleForMotionEstimation();
    /*!
        \param picSize video size in pixels
    */
    virtual void onStreamResolutionChanged( int channelNumber, const QSize& picSize );
protected:
    mutable QnMutex m_livemutex;
private:
    float getDefaultFps() const;
private:
    // NOTE: m_newLiveParams are going to update a little before the actual stream gets reopend
    // TODO: find out the way to keep it in sync besides pleaseReopenStream() call (which causes delay)
    QnLiveStreamParams m_newLiveParams;

    bool m_prevCameraControlDisabled;
    bool m_qualityUpdatedAtLeastOnce;
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
    QSize m_videoResolutionByChannelNumber[CL_MAX_CHANNELS];
    int m_softMotionLastChannel;
    QnConstResourceVideoLayoutPtr m_layout;
    QnPhysicalCameraResourcePtr m_cameraRes;
    bool m_isPhysicalResource;
    simd128i *m_motionMaskBinData[CL_MAX_CHANNELS];
    QElapsedTimer m_resolutionCheckTimer;
    int m_framesSincePrevMediaStreamCheck;

    void updateStreamResolution( int channelNumber, const QSize& newResolution );
    void extractMediaStreamParams(
        const QnCompressedVideoDataPtr& videoData,
        QSize* const newResolution,
        std::map<QString, QString>* const customStreamParams = nullptr );
    void saveMediaStreamParamsIfNeeded( const QnCompressedVideoDataPtr& videoData );
    void saveBitrateIfNotExists( const QnCompressedVideoDataPtr& videoData,
                                 const QnLiveStreamParams& liveParams );
    void extractSpsPps(
        const QnCompressedVideoDataPtr& videoData,
        QSize* const newResolution,
        std::map<QString, QString>* const customStreamParams );

private:
    QnAbstractVideoCamera* m_owner;
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif //live_strem_provider_h_1508
