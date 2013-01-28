#ifndef __PROGRESSIVE_DOWNLOADING_SERVER_H__
#define __PROGRESSIVE_DOWNLOADING_SERVER_H__

#include "utils/network/tcp_connection_processor.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/network/tcp_listener.h"

class QnFfmpegTranscoder;

class QnProgressiveDownloadingServer : public QnTcpListener
{
public:
    explicit QnProgressiveDownloadingServer(const QHostAddress& address, int port);
    virtual ~QnProgressiveDownloadingServer();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) override;
};

class QnProgressiveDownloadingConsumerPrivate;

class QnProgressiveDownloadingConsumer: virtual public QnTCPConnectionProcessor
{
public:
    QnProgressiveDownloadingConsumer(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnProgressiveDownloadingConsumer();

    QnFfmpegTranscoder* getTranscoder();
    int getVideoStreamResolution() const;
protected:
    virtual void run() override;
private:
    static QByteArray getMimeType(const QByteArray& streamingFormat);
    void updateCodecByFormat(const QByteArray& streamingFormat);
private:
    Q_DECLARE_PRIVATE(QnProgressiveDownloadingConsumer);
};

#endif // __PROGRESSIVE_DOWNLOADING_SERVER_H__
