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

protected:

private:
    void run(); // in a loop: takes images from camera and put into queue

    CLAdaptiveSleep m_fpsSleep;
};

#endif // client_pull_stream_reader_h1226
