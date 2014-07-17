#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226

#ifdef ENABLE_DATA_PROVIDERS

#include "media_streamdataprovider.h"
#include "utils/common/adaptive_sleep.h"
#include "core/dataprovider/live_stream_provider.h"

struct QnAbstractMediaData;

class QnClientPullMediaStreamProvider : public QnLiveStreamProvider
{
    Q_OBJECT;

public:
    QnClientPullMediaStreamProvider(const QnResourcePtr& dev);
    virtual ~QnClientPullMediaStreamProvider() {stop();}

protected:
    bool canChangeStatus() const;
    virtual void beforeRun() override;

    virtual QnAbstractMediaDataPtr getNextData() = 0;
private:
    void run(); // in a loop: takes images from camera and put into queue

    QnAdaptiveSleep m_fpsSleep;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // client_pull_stream_reader_h1226
