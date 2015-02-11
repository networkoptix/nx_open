#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#ifdef ENABLE_DATA_PROVIDERS

#include <QtCore/QElapsedTimer>
#include <QtCore/QObject>

#include "motion/motion_estimation.h"
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

class QnLiveStreamProvider: public QnAbstractMediaStreamDataProvider
{
public:
    QnLiveStreamProvider(const QnResourcePtr& res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(Qn::ConnectionRole role) override;
    Qn::ConnectionRole getRole() const;


    void setSecondaryQuality(Qn::SecondStreamQuality  quality);
    virtual void setQuality(Qn::StreamQuality q);
    Qn::StreamQuality getQuality() const;
    virtual void setCameraControlDisabled(bool value);

    // for live providers only 
    virtual void setFps(float f);
    float getFps() const;
    bool isMaxFps() const;

    void onPrimaryFpsUpdated(int newFps);

    // I assume this function is called once per video frame 
    bool needMetaData(); 

    virtual void onGotVideoFrame(const QnCompressedVideoDataPtr& videoData);

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
    virtual bool isCameraControlRequired() const;
    void filterMotionByMask(const QnMetaDataV1Ptr& motion);
    void updateSoftwareMotionStreamNum();

    void setOwner(QnAbstractVideoCamera* owner);
protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;

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
    //int m_NumaberOfVideoChannels;
    Qn::StreamQuality m_quality;
    bool m_prevCameraControlDisabled;
    bool m_qualityUpdatedAtLeastOnce;

    mutable float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

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
    Qn::SecondStreamQuality  m_secondaryQuality;
    simd128i *m_motionMaskBinData[CL_MAX_CHANNELS];
    QElapsedTimer m_resolutionCheckTimer;
    int m_framesSincePrevMediaStreamCheck;

    void updateStreamResolution( int channelNumber, const QSize& newResolution );
    void extractCodedPictureResolution( const QnCompressedVideoDataPtr& videoData, QSize* const newResolution );
    void saveMediaStreamParamsIfNeeded( const QnCompressedVideoDataPtr& videoData );
private:
    QnAbstractVideoCamera* m_owner;
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif // ENABLE_DATA_PROVIDERS

#endif //live_strem_provider_h_1508
