#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226


#include "media_streamdataprovider.h"
#include "utils/common/adaptive_sleep.h"
#include "core/dataprovider/live_stream_provider.h"

struct QnAbstractMediaData;

class QnClientPullMediaStreamProvider : public QnLiveStreamProvider
{
    Q_OBJECT;

public:
    QnClientPullMediaStreamProvider(QnResourcePtr dev);
    virtual ~QnClientPullMediaStreamProvider() {stop();}

protected:
    bool canChangeStatus() const;

private:
    void run(); // in a loop: takes images from camera and put into queue
    virtual void beforeRun() override;

    QnAdaptiveSleep m_fpsSleep;
};

#endif // client_pull_stream_reader_h1226
