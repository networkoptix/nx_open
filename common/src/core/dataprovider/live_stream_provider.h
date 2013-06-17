#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include <QObject>
#include "../resource/media_resource.h"
#include "core/datapacket/media_data_packet.h"
#include "motion/motion_estimation.h"
#include "../resource/motion_window.h"
#include "core/resource/resource_fwd.h"
#include "media_streamdataprovider.h"

static const int  META_DATA_DURATION_MS = 300;
static const int MIN_SECOND_STREAM_FPS = 2;
//#define DESIRED_SECOND_STREAM_FPS (7)
//#define MIN_SECOND_STREAM_FPS (2)


class QnLiveStreamProvider: public QnAbstractMediaStreamDataProvider
{
public:
    QnLiveStreamProvider(QnResourcePtr res);
    virtual ~QnLiveStreamProvider();

    virtual void setRole(QnResource::ConnectionRole role) override;
    QnResource::ConnectionRole getRole() const;


    void setSecondaryQuality(QnSecondaryStreamQuality  quality);
    virtual void setQuality(QnStreamQuality q);
    QnStreamQuality getQuality() const;

    // for live providers only 
    virtual void setFps(float f);
    float getFps() const;
    bool isMaxFps() const;

    void onPrimaryFpsUpdated(int newFps);

    // I assume this function is called once per video frame 
    bool needMetaData(); 

    void onGotVideoFrame(QnCompressedVideoDataPtr videoData);

    void setUseSoftwareMotion(bool value);

    void updateSoftwareMotion();
    bool canChangeStatus() const { return m_role == QnResource::Role_LiveVideo && m_isPhysicalResource; }

    virtual bool secondaryResolutionIsLarge() const { return false; }

    static bool hasRunningLiveProvider(QnNetworkResourcePtr netRes);
protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;

    QnMetaDataV1Ptr getMetaData();
    virtual QnMetaDataV1Ptr getCameraMetadata();
    QnResource::ConnectionRole roleForMotionEstimation();
protected:
    mutable QMutex m_livemutex;


private:
    //int m_NumaberOfVideoChannels;
    QnStreamQuality m_quality;
    bool m_qualityUpdatedAtLeastOnce;

    mutable float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    QnResource::ConnectionRole m_softMotionRole;
    QnMotionEstimation m_motionEstimation[CL_MAX_CHANNELS];
    int m_softMotionLastChannel;
    const QnResourceVideoLayout* m_layout;
    QnPhysicalCameraResourcePtr m_cameraRes;
    bool m_isPhysicalResource;
    QnSecondaryStreamQuality  m_secondaryQuality;
};

#endif //live_strem_provider_h_1508