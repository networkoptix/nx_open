#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include <QObject>
#include <QHttpRequestHeader>
#include <QNetworkInterface>
#include "utils/common/long_runnable.h"

class TCPSocket;
class QnTCPConnectionProcessor;

class QnTcpListenerPrivate;

class QnTcpListener: public QnLongRunnable
{
public:
    bool authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    explicit QnTcpListener(const QHostAddress& address, int port, int maxConnections = 1000);
    virtual ~QnTcpListener();

    void updatePort(int newPort);
    void* getOpenSSLContext();
    bool enableSSLMode();

    int getPort() const;

    /** Remove ownership from connection.*/
    void removeOwnership(QnLongRunnable* processor);
protected:
    virtual void run();
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) = 0;

private:
    void removeDisconnectedConnections();
    void removeAllConnections();

protected:
    Q_DECLARE_PRIVATE(QnTcpListener);

    QnTcpListenerPrivate *d_ptr;
};



#endif // __TCP_LISTENER_H__
