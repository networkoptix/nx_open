#ifndef live_strem_provider_h_1508
#define live_strem_provider_h_1508

#include "../resource/media_resource.h"

#define META_DATA_DURATION_MS 300
#define MAX_LIVE_FPS 10000000.0

class QnLiveStreamProvider 
{
public:
    QnLiveStreamProvider();
    virtual ~QnLiveStreamProvider();

    virtual void setQuality(QnStreamQuality q);
    QnStreamQuality getQuality() const;

    // for live providers only 
    virtual void setFps(float f);
    float getFps() const;
    bool isMaxFps() const;

    bool needMetaData() const; // used for live only 

protected:

    virtual void updateStreamParamsBasedOnQuality(){};
    virtual void updateStreamParamsBasedOnFps(){};

protected:

    mutable QMutex m_livemutex;

    //int m_NumaberOfVideoChannels;
    QnStreamQuality m_quality;

    float m_fps; //used only for live providers
    unsigned int m_framesSinceLastMetaData; // used only for live providers
    QTime m_timeSinceLastMetaData; //used only for live providers



};

#endif //live_strem_provider_h_1508