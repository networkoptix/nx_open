#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include <QtCore/QObject>
#include "../resource/media_resource.h"
#include "motion/motion_estimation.h"
#include "../resource/motion_window.h"
#include "core/resource/resource_fwd.h"
#include "media_streamdataprovider.h"

static const int  META_DATA_DURATION_MS = 300;
static const int MIN_SECOND_STREAM_FPS = 2;
static const int MAX_PRIMARY_RES_FOR_SOFT_MOTION = 720 * 576;
//#define DESIRED_SECOND_STREAM_FPS (7)
//#define MIN_SECOND_STREAM_FPS (2)


class QnLiveStreamProvider: public QnAbstractMediaStreamDataProvider
{
public:
    QnLiveStreamProvider(QnResourcePtr res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(QnResource::ConnectionRole role) override;
    QnResource::ConnectionRole getRole() const;


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

    virtual void onGotVideoFrame(QnCompressedVideoDataPtr videoData);

    void setUseSoftwareMotion(bool value);

    virtual void updateSoftwareMotion();
    bool canChangeStatus() const;

    virtual bool secondaryResolutionIsLarge() const { return false; }

    static bool hasRunningLiveProvider(QnNetworkResourcePtr netRes);

    /*!
        Start provider if not running yet.
        @param canTouchCameraSettings can control camera settings if true
    */
    void startIfNotRunning();

    bool isCameraControlDisabled() const;
    void filterMotionByMask(QnMetaDataV1Ptr motion);
protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;

    QnMetaDataV1Ptr getMetaData();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    virtual QnResource::ConnectionRole roleForMotionEstimation();
    /*!
        \param picSize video size in pixels
    */
    virtual void onStreamResolutionChanged( int channelNumber, const QSize& picSize );

protected:
    mutable QMutex m_livemutex;


private:
    //int m_NumaberOfVideoChannels;
    Qn::StreamQuality m_quality;
    bool m_prevCameraControlDisabled;
    bool m_qualityUpdatedAtLeastOnce;

    mutable float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    QnResource::ConnectionRole m_softMotionRole;
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
};

typedef QSharedPointer<QnLiveStreamProvider> QnLiveStreamProviderPtr;

#endif //live_strem_provider_h_1508
