#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226


#include "core/datapacket/media_data_packet.h"
#include "media_streamdataprovider.h"
#include "utils/common/adaptive_sleep.h"

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
protected:
    bool canChangeStatus() const;

private:
    void run(); // in a loop: takes images from camera and put into queue
    virtual void beforeRun() override;

    QnAdaptiveSleep m_fpsSleep;

    float m_fps; //used only for live providers
};

#endif // client_pull_stream_reader_h1226
