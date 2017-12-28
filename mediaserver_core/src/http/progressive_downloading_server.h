#ifndef __PROGRESSIVE_DOWNLOADING_SERVER_H__
#define __PROGRESSIVE_DOWNLOADING_SERVER_H__

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"
#include <nx/utils/timer_manager.h>

class QnFfmpegTranscoder;

class QnProgressiveDownloadingConsumerPrivate;

class QnProgressiveDownloadingConsumer
:
    virtual public QnTCPConnectionProcessor,
    public nx::utils::TimerEventHandler
{
public:
    static bool doesPathEndWithCameraId() { return true; } //< See the base class method.

    QnProgressiveDownloadingConsumer(QSharedPointer<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
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
