#ifndef client_pull_stream_reader_h1226
#define client_pull_stream_reader_h1226

#ifdef ENABLE_DATA_PROVIDERS

#include <nx/streaming/abstract_media_stream_data_provider.h>
#include <utils/common/adaptive_sleep.h>
#include "live_stream_provider.h"

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
    virtual bool isCameraControlRequired() const = 0;

    virtual QnAbstractMediaDataPtr getNextData() = 0;
private:
    void run(); // in a loop: takes images from camera and put into queue

    QnAdaptiveSleep m_fpsSleep;
};

#endif // ENABLE_DATA_PROVIDERS

#endif // client_pull_stream_reader_h1226
