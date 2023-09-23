// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tcp_listener.h"

#include <atomic>
#include <regex>

#include <nx/metrics/metrics_storage.h>
#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>
#include <nx/vms/common/system_context.h>

#include "tcp_connection_processor.h"

namespace {
    const std::chrono::seconds kDefaultSocketTimeout(5);
}

//-------------------------------------------------------------------------------------------------
// QnTcpListenerPrivate

class QnTcpListenerPrivate
{
public:
    nx::network::SocketGlobals::InitGuard socketGlobalsInitGuard;
    std::unique_ptr<nx::network::AbstractStreamServerSocket> serverSocket;
    nx::network::SocketAddress localEndpoint;
    QList<QnLongRunnable*> connections;
    QByteArray authDigest;
    mutable nx::Mutex mutex;
    nx::Mutex connectionMtx;
    std::atomic<int> newPort{};
    QHostAddress serverAddress;
    std::atomic<int> localPort = 0;
    bool useSSL = false;
    int maxConnections = 0;
    bool ddosWarned = false;
    SystemError::ErrorCode lastError = SystemError::noError;
    bool isStopped = false;

    QString pathIgnorePrefix;
};

//-------------------------------------------------------------------------------------------------
// QnTcpListener

bool QnTcpListener::authenticate(
    const nx::network::http::Request& request, nx::network::http::Response& response) const
{
    Q_D(const QnTcpListener);
    if (d->authDigest.isEmpty())
        return true;
    nx::network::http::HttpHeaders::const_iterator authorizationIter = request.headers.find("Authorization");
    if(authorizationIter == request.headers.end())
        return false;

    const auto [data, dataSize] = nx::utils::split_n<2>(authorizationIter->second, ' ');
    bool rez = false;
    if (nx::utils::stricmp(data[0], "basic") == 0 && dataSize > 1)
        rez = data[1] == d->authDigest;
    if (!rez)
    {
        nx::network::http::insertOrReplaceHeader(&response.headers,
            nx::network::http::HttpHeader("WWW-Authenticate", "Basic realm=\"Secure Area\""));
    }
    return rez;
}

void QnTcpListener::setAuth(const QByteArray& userName, const QByteArray& password)
{
    Q_D(QnTcpListener);
    QByteArray digest = userName + QByteArray(":") + password;
    d->authDigest = digest.toBase64();
}

QnTcpListener::QnTcpListener(
    nx::vms::common::SystemContext* systemContext,
    int maxTcpRequestSize,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSSL)
    :
    nx::vms::common::SystemContextAware(systemContext),
    d_ptr(new QnTcpListenerPrivate()),
    m_maxTcpRequestSize(maxTcpRequestSize)
{
    Q_D(QnTcpListener);
    d->serverAddress = address;
    d->localPort = port;
    d->maxConnections = maxConnections;
    d->useSSL = useSSL;
}

QnTcpListener::~QnTcpListener()
{
    stop();
    destroyServerSocket();
    delete d_ptr;
}

void QnTcpListener::stop()
{
    Q_D(QnTcpListener);
    d->isStopped = true;
    QnLongRunnable::stop();
}

static const int kSocketAcceptTimeoutMs = 250;

bool QnTcpListener::bindToLocalAddress()
{
    Q_D(QnTcpListener);

    const nx::network::SocketAddress localAddress(
        d->serverAddress.toString().toStdString(), d->localPort);
    auto serverSocket = createAndPrepareSocket(d->useSSL, localAddress);
    if (!serverSocket
        || !serverSocket->setRecvTimeout(kSocketAcceptTimeoutMs))
    {
        NX_ERROR(this, "Unable to bind and listen on %1: %2", localAddress, lastError());
        return false;
    }
    d->serverSocket = std::move(serverSocket);

    {
        NX_MUTEX_LOCKER lk(&d->mutex);
        d->localEndpoint = d->serverSocket->getLocalAddress();
        d->localPort = d->localEndpoint.port;
    }

    NX_INFO(this, "Server started at %1", d->localEndpoint);
    return true;
}

void QnTcpListener::doPeriodicTasks()
{
    removeDisconnectedConnections();
}

std::unique_ptr<nx::network::AbstractStreamServerSocket> QnTcpListener::createAndPrepareSocket(
    bool sslNeeded,
    const nx::network::SocketAddress& localAddress)
{
    auto serverSocket = nx::network::SocketFactory::createStreamServerSocket(sslNeeded);

    if (localAddress.port)
    {
        if (!serverSocket->setReuseAddrFlag(true) ||
            !serverSocket->setReusePortFlag(true))
        {
            setLastError(SystemError::getLastOSErrorCode());
            return nullptr;
        }
    }

    if (!serverSocket->bind(localAddress) ||
        !serverSocket->listen())
    {
        setLastError(SystemError::getLastOSErrorCode());
        return nullptr;
    }

    return serverSocket;
}

void QnTcpListener::destroyServerSocket()
{
    Q_D(QnTcpListener);
    if (d->serverSocket)
        d->serverSocket->pleaseStopSync();
    d->serverSocket.reset();
}

void QnTcpListener::removeDisconnectedConnections()
{
    Q_D(QnTcpListener);

    QVector<QnLongRunnable*> toDeleteList;

    {
        NX_MUTEX_LOCKER lock(&d->connectionMtx);

        QList<QnLongRunnable*>::iterator itr = d->connections.begin();
        while (itr != d->connections.end())
        {
            QnLongRunnable* processor = *itr;
            if (!processor->isRunning())
            {
                toDeleteList << processor;
                itr = d->connections.erase(itr);
            }
            else
            {
                ++itr;
            }
        }
    }

    for(QnLongRunnable* processor: toDeleteList)
        delete processor;
}

void QnTcpListener::removeOwnership(QnLongRunnable* processor)
{
    Q_D(QnTcpListener);
    NX_MUTEX_LOCKER lock(&d->connectionMtx);
    for (QList<QnLongRunnable*>::iterator itr = d->connections.begin();
        itr != d->connections.end(); ++itr)
    {
        if (processor == *itr)
        {
            d->connections.erase(itr);
            break;
        }
    }
}

void QnTcpListener::addOwnership(QnLongRunnable* processor)
{
    Q_D(QnTcpListener);

    NX_MUTEX_LOCKER lock(&d->connectionMtx);
    d->connections << processor;
}

bool QnTcpListener::isSslEnabled() const
{
    Q_D(const QnTcpListener);
    return d->useSSL;
}

SystemError::ErrorCode QnTcpListener::lastError() const
{
    Q_D(const QnTcpListener);
    return d->lastError;
}

void QnTcpListener::setLastError(SystemError::ErrorCode error)
{
    Q_D(QnTcpListener);
    d->lastError = error;
}

void QnTcpListener::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    NX_DEBUG(this, "pleaseStop() called");
}

void QnTcpListener::removeAllConnections()
{
    Q_D(QnTcpListener);

    QList<QnLongRunnable*> oldConnections;
    {
        NX_MUTEX_LOCKER lock(&d->connectionMtx);
        std::swap(d->connections, oldConnections);
    }

    for (const auto& processor: oldConnections)
        processor->pleaseStop();

    for (const auto& processor: oldConnections)
    {
        NX_DEBUG(this, "Stopping processor (sysThreadID %1)", processor->systemThreadId());
        delete processor;
    }
}

void QnTcpListener::updatePort(int newPort)
{
    Q_D(QnTcpListener);
    d->newPort = newPort;
}

void QnTcpListener::waitForPortUpdated()
{
    Q_D(QnTcpListener);
    while (d->newPort != 0)
        msleep(1);
}

void QnTcpListener::run()
{
    Q_D(QnTcpListener);

    initSystemThreadId();

    d->ddosWarned = false;
    if(!d->serverSocket)
        bindToLocalAddress();

    NX_DEBUG(this, "Entered run(). %1:%2, system thread id %3",
        d->serverAddress, d->localPort, systemThreadId());

    try
    {
        if (!d->serverSocket)
            m_needStop = true;
        while (!needToStop())
        {
            if (d->newPort)
            {
                int oldPort = d->localPort;
                d->localPort.store(d->newPort);
                NX_INFO(this, "(%1:%2). Switching port to: %3",
                    d->serverAddress, oldPort, d->localPort);
                // TODO: Add comment why this code is commented out.
                //removeAllConnections();
                destroyServerSocket();
                if(!bindToLocalAddress())
                {
                    QThread::msleep(1000);
                    continue;
                }
                NX_INFO(this, "(%1:%2). Switched to port %3",
                    d->serverAddress, oldPort, d->localPort);
                int currentValue = d->localPort;

                // Reset newPort if no more changes.
                d->newPort.compare_exchange_strong(currentValue, 0);
                emit portChanged();
            }

            if (d->localPort == 0 && d->serverSocket)
                d->localPort = d->serverSocket->getLocalAddress().port;

            doPeriodicTasks();
            auto clientSocket = d->serverSocket->accept();
            if (clientSocket)
            {
                if (systemContext())
                {
                    systemContext()->metrics()->tcpConnections().total()++;
                    clientSocket->setBeforeDestroyCallback(
                        [weakRef = std::weak_ptr<nx::metrics::Storage>(systemContext()->metrics())]
                        ()
                        {
                            if (auto metrics = weakRef.lock())
                                metrics->tcpConnections().total()--;
                        });
                }
                processNewConnection(std::move(clientSocket));
            }
            else
            {
                const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                if (prevErrorCode != SystemError::timedOut
                    && prevErrorCode != SystemError::again
                    && prevErrorCode != SystemError::interrupted)
                {
                    NX_WARNING(this, "(%1:%2). Accept failed: %3 (%4)",
                        d->serverAddress,
                        d->localPort,
                        prevErrorCode,
                        SystemError::toString(prevErrorCode));
                    QThread::msleep(1000);
                    int zero = 0;
                    d->newPort.compare_exchange_strong(zero, d->localPort); //< reopen tcp socket
                }
                else
                {
                    NX_VERBOSE(this, "(%1:%2). Accept event: %3 (%4), continue accepting",
                        d->serverAddress,
                        d->localPort,
                        prevErrorCode,
                        SystemError::toString(prevErrorCode));
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        NX_ERROR(this, "Exception in TCPListener (%1:%2). %3",
            d->serverAddress, d->localPort, e.what());
    }

    NX_DEBUG(this, "(%1:%2). Removing all connections before stop",
        d->serverAddress, d->localPort);
    removeAllConnections();
    destroyServerSocket();
    NX_DEBUG(this, "Exiting run(). %1:%2", d->serverAddress, d->localPort);
}

void QnTcpListener::processNewConnection(std::unique_ptr<nx::network::AbstractStreamSocket> socket)
{
    Q_D(QnTcpListener);
    {
        NX_MUTEX_LOCKER lock(&d->connectionMtx);
        if (d->isStopped)
            return;
    }

    if (d->connections.size() > d->maxConnections)
    {
        if (!d->ddosWarned)
        {
            NX_WARNING(this,
                "Amount of TCP connections reached %1 of %2. Possible ddos attack!"
                " Reject %3 from %4",
                d->connections.size(), d->maxConnections, socket, socket->getForeignAddress());
            d->ddosWarned = true;
        }
        return;
    }
    d->ddosWarned = false;
    NX_VERBOSE(this, "New client connection %1 from %2", socket, socket->getForeignAddress());

    socket->setRecvTimeout(kDefaultSocketTimeout);
    socket->setSendTimeout(kDefaultSocketTimeout);
    QnTCPConnectionProcessor* processor =
        createRequestProcessor(std::move(socket), m_maxTcpRequestSize);

    NX_MUTEX_LOCKER lock(&d->connectionMtx);
    d->connections << processor;
    processor->start();
}

int QnTcpListener::getPort() const
{
    Q_D(const QnTcpListener);
    return d->localPort;
}

nx::network::SocketAddress QnTcpListener::getLocalEndpoint() const
{
    Q_D(const QnTcpListener);
    NX_MUTEX_LOCKER lk(&d->mutex);
    return d->localEndpoint;
}

void QnTcpListener::setPathIgnorePrefix(const QString& path)
{
    Q_D(QnTcpListener);
    d->pathIgnorePrefix = path;
}

QString QnTcpListener::normalizedPath(const QString& path)
{
    Q_D(const QnTcpListener);
    return nx::network::url::normalizedPath(path, d->pathIgnorePrefix);
}

const QString& QnTcpListener::pathIgnorePrefix() const
{
    Q_D(const QnTcpListener);
    return d->pathIgnorePrefix;
}

bool QnTcpListener::isLargeRequestAllowed(const QString& path)
{
    const auto toMatch = path.toStdString();
    for (const auto& [pattern, regex]: m_allowedLargeRequestPatterns)
    {
        if (std::regex_match(toMatch, regex))
            return true;
    }

    return false;
}

void QnTcpListener::allowLargeRequest(const QString& pattern)
{
    m_allowedLargeRequestPatterns.emplace(pattern.toStdString(), pattern.toStdString());
}
