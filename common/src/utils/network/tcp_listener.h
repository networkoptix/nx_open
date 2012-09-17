#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include <QObject>
#include <QHttpRequestHeader>
#include <QNetworkInterface>
#include "utils/common/longrunnable.h"
#include "utils/common/pimpl.h"

class TCPSocket;
class QnTCPConnectionProcessor;

class QnTcpListener: public QnLongRunnable
{
public:
    bool authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    explicit QnTcpListener(const QHostAddress& address, int port);
    virtual ~QnTcpListener();

    void updatePort(int newPort);
    void* getOpenSSLContext();
    bool enableSSLMode();
protected:
    virtual void run();
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) = 0;
private:
    void removeDisconnectedConnections();
    void removeAllConnections();
private:
    QN_DECLARE_PRIVATE(QnTcpListener);
};



#endif // __TCP_LISTENER_H__
