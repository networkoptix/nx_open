#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include "../resource/media_resource.h"

#define META_DATA_DURATION_MS 300


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

protected:

    virtual void updateStreamParamsBasedOnQuality() = 0;
    virtual void updateStreamParamsBasedOnFps() = 0;




    
protected:
    mutable QMutex m_livemutex;


private:
    //int m_NumaberOfVideoChannels;
    QnStreamQuality m_quality;

    float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers

    QnResource::ConnectionRole m_role;

};

#endif //live_strem_provider_h_1508