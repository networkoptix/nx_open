#ifndef __VMAX480_TCP_SERVER_H__
#define __VMAX480_TCP_SERVER_H__

#include <QMap>
#include <QMutex>

#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "core/datapacket/media_data_packet.h"

class TCPSocket;
class QnTCPConnectionProcessor;
class QnVMax480LiveProvider;

class QnTcpListenerPrivate;
class QnVMax480ConnectionProcessor;

class QnVMax480Server: public QnTcpListener
{
public:
    explicit QnVMax480Server();
    virtual ~QnVMax480Server();

    static QnVMax480Server* instance();

    QString registerProvider(QnVMax480LiveProvider* provider);
    void unregisterProvider(QnVMax480LiveProvider* provider);

    void registerConnectionProcessor(const QString& tcpID, QnVMax480ConnectionProcessor* processor);
    void unregisterConnectionProcessor(const QString& tcpID);

    QnVMax480ConnectionProcessor* waitForConnection(const QString& tcpID, int timeout);

    void onGotData(const QString& tcpID, QnAbstractMediaDataPtr mediaData);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) override;
private:
    QMutex m_providersMutex;
    QMap<QString, QnVMax480LiveProvider*> m_providers;
    QMap<QString, QnVMax480ConnectionProcessor*> m_processors;
};

class QnVMax480ConnectionProcessorPrivate;

class QnVMax480ConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnVMax480ConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnVMax480ConnectionProcessor();

    void connect(const QUrl& url, const QAuthenticator& auth);
    void disconnect();
protected:
    virtual void run() override;
private:
    Q_DECLARE_PRIVATE(QnVMax480ConnectionProcessor);
};

#endif // __VMAX480_TCP_SERVER_H__
