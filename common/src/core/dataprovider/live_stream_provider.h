#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include <QObject>
#include "../resource/media_resource.h"
#include "core/datapacket/mediadatapacket.h"

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

    void onGotVideoFrame();

protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;
    
    virtual QnMetaDataV1Ptr getMetaData();
    
protected:
    mutable QMutex m_livemutex;


private:
    //int m_NumaberOfVideoChannels;
    QnStreamQuality m_quality;

    mutable float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    QnResource::ConnectionRole m_role;

};

#endif //live_strem_provider_h_1508