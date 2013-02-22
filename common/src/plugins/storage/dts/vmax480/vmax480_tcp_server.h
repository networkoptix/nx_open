#ifndef __VMAX480_TCP_SERVER_H__
#define __VMAX480_TCP_SERVER_H__

#include <QMutex>
#include <QMap>
#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"
#include "core/datapacket/media_data_packet.h"

class QnVMax480LiveProvider;
class QnVMax480ConnectionProcessor;
class QnVMax480ConnectionProcessorPrivate;

class QnVMax480Server: public QnTcpListener
{
public:
    QnVMax480Server();

    static QnVMax480Server* instance();

    bool waitForConnection(const QString& tcpID, int timeoutMs);

    QString registerProvider(QnVMax480LiveProvider* provider);
    void unregisterProvider(QnVMax480LiveProvider* provider);

    void registerConnection(const QString& tcpID, QnVMax480ConnectionProcessor* connection);
    void unregisterConnection(const QString& tcpID);

    void vMaxConnect(const QString& tcpID, const QString& url, const QAuthenticator& auth);
    void vMaxDisconnect(const QString& tcpID);

    void onGotData(const QString& tcpID, QnAbstractMediaDataPtr media);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) override;
private:
    QMap<QString, QnVMax480LiveProvider*> m_providers;
    QMap<QString, QnVMax480ConnectionProcessor*> m_connections;
    QMutex m_mutexProvider;
    QMutex m_mutexConnection;
};

class QnVMax480ConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnVMax480ConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnVMax480ConnectionProcessor();

    void vMaxConnect(const QString& url, const QAuthenticator& auth);
    void vMaxDisconnect();
protected:
    virtual void run() override;
private:
    bool readBuffer(quint8* buffer, int size);
private:
    Q_DECLARE_PRIVATE(QnVMax480ConnectionProcessor);
};

#endif // __VMAX480_TCP_SERVER_H__
