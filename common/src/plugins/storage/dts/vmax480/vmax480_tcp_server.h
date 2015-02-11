#ifndef __VMAX480_TCP_SERVER_H__
#define __VMAX480_TCP_SERVER_H__

#ifdef ENABLE_VMAX

#include <utils/common/mutex.h>
#include <QtCore/QMap>
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
    virtual ~QnVMax480Server();

    static QnVMax480Server* instance();

    bool waitForConnection(const QString& tcpID, int timeoutMs);

    QString registerProvider(VMaxStreamFetcher* provider);
    void unregisterProvider(VMaxStreamFetcher* provider);

    VMaxStreamFetcher* bindConnection(const QString& tcpID, QnVMax480ConnectionProcessor* connection);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) override;
private:
    QMap<QString, VMaxStreamFetcher*> m_providers;
    QnMutex m_mutexProvider;
};

class QnVMax480ConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnVMax480ConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnVMax480ConnectionProcessor();

    void vMaxConnect(const QString& url, int channel, const QAuthenticator& auth, bool isLive);
    void vMaxDisconnect();
    void vMaxArchivePlay(qint64 timeUsec, quint8 newSequence, int speed);
    void vmaxPlayRange(const QList<qint64>& pointsUsec, quint8 sequence);
    void vMaxRequestMonthInfo(const QDate& month);
    void vMaxRequestDayInfo(int dayNum);
    void vMaxRequestRange();

    void vMaxAddChannel(int channel);
    void vMaxRemoveChannel(int channel);
protected:
    virtual void run() override;
private:
    bool readBuffer(quint8* buffer, int size);
private:
    Q_DECLARE_PRIVATE(QnVMax480ConnectionProcessor);
};

#endif // #ifdef ENABLE_VMAX
#endif // __VMAX480_TCP_SERVER_H__
