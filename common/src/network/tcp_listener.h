#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include <QtCore/QObject>

#include <QtNetwork/QNetworkInterface>

#include "utils/common/long_runnable.h"

#include <nx/network/http/httptypes.h>
#include <nx/network/abstract_socket.h>


class TCPSocket;
class QnTCPConnectionProcessor;

class QnTcpListenerPrivate;

class QnTcpListener: public QnLongRunnable
{
public:
    static const int DEFAULT_MAX_CONNECTIONS = 1000;

    bool authenticate(const nx_http::Request& headers, nx_http::Response& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    explicit QnTcpListener( const QHostAddress& address, int port, int maxConnections = DEFAULT_MAX_CONNECTIONS, bool useSSL = true );
    virtual ~QnTcpListener();

    //!Bind to local address:port, specified in constructor
    /*!
        \return \a false if failed to bind
    */
    bool bindToLocalAddress();

    void updatePort(int newPort);
    void waitForPortUpdated();

    int getPort() const;

    /** Remove ownership from connection.*/
    void removeOwnership(QnLongRunnable* processor);

    void addOwnership(QnLongRunnable* processor);

    bool isSslEnabled() const;

    static void setDefaultPage(const QByteArray& path);
    static QByteArray defaultPage();

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run();
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket) = 0;
    virtual void doPeriodicTasks();
    /** Called to create server socket.
        This method is supposed to bind socket to \a localAddress and call \a listen
        \note If \a nullptr has been returned, system error code is set to proper error
    */
    virtual AbstractStreamServerSocket* createAndPrepareSocket(
        bool sslNeeded,
        const SocketAddress& localAddress);
    virtual void destroyServerSocket(AbstractStreamServerSocket* serverSocket);

private:
    void removeDisconnectedConnections();
    void removeAllConnections();

protected:
    Q_DECLARE_PRIVATE(QnTcpListener);

    QnTcpListenerPrivate *d_ptr;
};



#endif // __TCP_LISTENER_H__
