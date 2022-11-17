// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <QtNetwork/QNetworkInterface>

#include "nx/utils/thread/long_runnable.h"

#include <nx/network/http/http_types.h>
#include <nx/network/abstract_socket.h>
#include <common/common_module_aware.h>


class QnTCPConnectionProcessor;

class QnTcpListenerPrivate;

class NX_VMS_COMMON_API QnTcpListener:
    public QnLongRunnable,
    public /*mixin*/ QnCommonModuleAware
{
    Q_OBJECT;

public:
    static const int DEFAULT_MAX_CONNECTIONS = 2000;

    bool authenticate(const nx::network::http::Request& headers, nx::network::http::Response& responseHeaders) const;

    void setAuth(const QByteArray& userName, const QByteArray& password);

    /*
     * Main call to process TCP connection. Usually tcp listener gets connections via accept() call.
     * But this function can be called manually to pass some external socket.
     * For instance, It is used for reverse connections.
     */
    void processNewConnection(std::unique_ptr<nx::network::AbstractStreamSocket> socket);

    explicit QnTcpListener(
        QnCommonModule* commonModule,
        int maxTcpRequestSize,
        const QHostAddress& address,
        int port,
        int maxConnections = DEFAULT_MAX_CONNECTIONS,
        bool useSSL = true );
    virtual ~QnTcpListener();

    //!Bind to local address:port, specified in constructor
    /*!
        \return \a false if failed to bind
    */
    bool bindToLocalAddress();

    void updatePort(int newPort);
    void waitForPortUpdated();

    int getPort() const;
    nx::network::SocketAddress getLocalEndpoint() const;

    /** Remove ownership from connection.*/
    void removeOwnership(QnLongRunnable* processor);

    void addOwnership(QnLongRunnable* processor);

    bool isSslEnabled() const;

    SystemError::ErrorCode lastError() const;

    /**
    * All handler will be matched as usual if request path starts with addition prefix
    */
    void setPathIgnorePrefix(const QString& path);
    const QString& pathIgnorePrefix() const;

    /**
     * Normalize url path, cut off web prefix and '/' chars.
     */
    QString normalizedPath(const QString& path);

    virtual void applyModToRequest(nx::network::http::Request* /*request*/) {}
    virtual void stop() override;
signals:
    void portChanged();

public slots:
    virtual void pleaseStop() override;

protected:
    virtual void run() override;
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket,
        int maxTcpRequestSize) = 0;
    virtual void doPeriodicTasks();
    /** Called to create server socket.
        This method is supposed to bind socket to \a localAddress and call \a listen
        \note If \a nullptr has been returned, system error code is set to proper error
    */
    virtual std::unique_ptr<nx::network::AbstractStreamServerSocket> createAndPrepareSocket(
        bool sslNeeded,
        const nx::network::SocketAddress& localAddress);

    void setLastError(SystemError::ErrorCode error);
protected:
    virtual void destroyServerSocket();
private:
    void removeDisconnectedConnections();
    void removeAllConnections();

protected:
    Q_DECLARE_PRIVATE(QnTcpListener);

    QnTcpListenerPrivate *d_ptr;

private:
    int m_maxTcpRequestSize;
};
