#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226


#include "core/datapacket/mediadatapacket.h"
#include "media_streamdataprovider.h"
#include "utils/common/adaptivesleep.h"

struct QnAbstractMediaData;

class QnClientPullMediaStreamProvider : public QnAbstractMediaStreamDataProvider
{
    Q_OBJECT;

public:
    QnClientPullMediaStreamProvider(QnResourcePtr dev);
    virtual ~QnClientPullMediaStreamProvider() {stop();}

    void setFps(float f);
    float getFps() const;
    bool isMaxFps() const;
    void setExtSync(QMutex* extSyncMutex);

protected:


private:
    void run(); // in a loop: takes images from camera and put into queue

    CLAdaptiveSleep m_fpsSleep;

    float m_fps; //used only for live providers
    QMutex* m_extSyncMutex;
};

#endif // client_pull_stream_reader_h1226
