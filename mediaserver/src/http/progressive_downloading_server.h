#ifndef __PROGRESSIVE_DOWNLOADING_SERVER_H__
#define __PROGRESSIVE_DOWNLOADING_SERVER_H__

#include "utils/network/tcp_connection_processor.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/network/tcp_listener.h"
#include "utils/common/timermanager.h"

class QnFfmpegTranscoder;

class QnProgressiveDownloadingConsumerPrivate;

class QnProgressiveDownloadingConsumer: virtual public QnTCPConnectionProcessor, public TimerEventHandler
{
public:
    QnProgressiveDownloadingConsumer(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnProgressiveDownloadingConsumer();

    QnFfmpegTranscoder* getTranscoder();
    int getVideoStreamResolution() const;
protected:
    virtual void run() override;
    virtual void onTimer( const quint64& timerID ) override;
private:
    static QByteArray getMimeType(const QByteArray& streamingFormat);
    void updateCodecByFormat(const QByteArray& streamingFormat);
private:
    Q_DECLARE_PRIVATE(QnProgressiveDownloadingConsumer);
};

#endif // __PROGRESSIVE_DOWNLOADING_SERVER_H__
