#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include <QObject>
#include <QHttpRequestHeader>
#include <QNetworkInterface>
#include "utils/common/longrunnable.h"
#include "utils/common/base.h"

class TCPSocket;

class QnTcpListener: public CLLongRunnable
{
public:
    bool authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    explicit QnTcpListener(const QHostAddress& address, int port);
    virtual ~QnTcpListener();

protected:
    virtual void run();
    virtual CLLongRunnable* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) = 0;
private:
    void removeDisconnectedConnections();
private:
    QN_DECLARE_PRIVATE(QnTcpListener);
};



#endif // __TCP_LISTENER_H__
