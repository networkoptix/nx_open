#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include <QObject>
#include "../resource/media_resource.h"
#include "core/datapacket/mediadatapacket.h"
#include "motion/motion_estimation.h"
#include "../resource/motion_window.h"

#define META_DATA_DURATION_MS 300

int bestBitrateKbps(QnStreamQuality q, QSize resolution, int fps);

class QnLiveStreamProvider
{
public:
    QnLiveStreamProvider();
    virtual ~QnLiveStreamProvider();

    void setRole(QnResource::ConnectionRole role);
    QnResource::ConnectionRole getRole() const;


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

    bool isSoftwareMotion() const;
    void setUseSoftwareMotion(bool value);

    void updateMotion();
protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;
    virtual void updateCameraMotion(const QnMotionRegion& region) {}

    QnMetaDataV1Ptr getMetaData();

    virtual QnMetaDataV1Ptr getCameraMetadata();
    QByteArray createSoftwareMotionMask(const QnMotionRegion& region);
protected:
    mutable QMutex m_livemutex;


private:
    //int m_NumaberOfVideoChannels;
    QnStreamQuality m_quality;

    mutable float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    QnResource::ConnectionRole m_role;
    QnMotionEstimation m_motionEstimation[CL_MAX_CHANNELS];
    bool m_softwareMotion;
    int m_softMotionLastChannel;
};

#endif //live_strem_provider_h_1508