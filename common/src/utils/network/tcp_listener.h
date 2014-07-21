#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include <QtCore/QObject>

#include <QtNetwork/QNetworkInterface>

#include "abstract_socket.h"
#include "utils/common/long_runnable.h"
#include "utils/network/http/httptypes.h"


class TCPSocket;
class QnTCPConnectionProcessor;

class QnTcpListenerPrivate;

class QnTcpListener: public QnLongRunnable
{
public:
    bool authenticate(const nx_http::Request& headers, nx_http::Response& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    explicit QnTcpListener(const QHostAddress& address, int port, int maxConnections = 1000);
    virtual ~QnTcpListener();

    //!Bind to local address:port, specified in constructor
    /*!
        \return \a false if failed to bind
    */
    bool bindToLocalAddress();

    void updatePort(int newPort);
    void enableSSLMode();

    int getPort() const;

    /** Remove ownership from connection.*/
    void removeOwnership(QnLongRunnable* processor);

    void addOwnership(QnLongRunnable* processor);

    static void setDefaultPage(const QByteArray& path);
    static QByteArray defaultPage();

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run();
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) = 0;
    virtual void doPeriodicTasks();
private:
    void removeDisconnectedConnections();
    void removeAllConnections();

protected:
    Q_DECLARE_PRIVATE(QnTcpListener);

    QnTcpListenerPrivate *d_ptr;
};



#endif // __TCP_LISTENER_H__
