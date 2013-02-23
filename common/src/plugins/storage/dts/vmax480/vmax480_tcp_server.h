#ifndef __VMAX480_TCP_SERVER_H__
#define __VMAX480_TCP_SERVER_H__

#include <QMutex>
#include <QMap>
#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"
#include "core/datapacket/media_data_packet.h"
#include "vmax480_stream_fetcher.h"

class VMaxStreamFetcher;
class QnVMax480ConnectionProcessor;
class QnVMax480ConnectionProcessorPrivate;

class QnVMax480Server: public QnTcpListener
{
public:
    QnVMax480Server();

    static QnVMax480Server* instance();

    bool waitForConnection(const QString& tcpID, int timeoutMs);

    QString registerProvider(VMaxStreamFetcher* provider);
    void unregisterProvider(VMaxStreamFetcher* provider);

    void registerConnection(const QString& tcpID, QnVMax480ConnectionProcessor* connection);
    void unregisterConnection(const QString& tcpID);

    void vMaxConnect(const QString& tcpID, const QString& url, const QAuthenticator& auth, bool isLive);
    void vMaxDisconnect(const QString& tcpID);
    void vMaxArchivePlay(const QString& tcpID, qint64 timeUsec, quint8 newSequence);

    void onGotData(const QString& tcpID, QnAbstractMediaDataPtr media);
    void onGotArchiveRange(const QString& tcpID, quint32 startDateTime, quint32 endDateTime);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) override;
private:
    QMap<QString, VMaxStreamFetcher*> m_providers;
    QMap<QString, QnVMax480ConnectionProcessor*> m_connections;
    QMutex m_mutexProvider;
    QMutex m_mutexConnection;
};

class QnVMax480ConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnVMax480ConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnVMax480ConnectionProcessor();

    void vMaxConnect(const QString& url, const QAuthenticator& auth, bool isLive);
    void vMaxDisconnect();
    void vMaxArchivePlay(qint64 timeUsec, quint8 newSequence);
protected:
    virtual void run() override;
private:
    bool readBuffer(quint8* buffer, int size);
private:
    Q_DECLARE_PRIVATE(QnVMax480ConnectionProcessor);
};

#endif // __VMAX480_TCP_SERVER_H__
