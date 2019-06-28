#include <atomic>

#include "tcp_connection_processor.h"
#include "tcp_listener.h"

#include <nx/utils/log/log.h>
#include <nx/utils/system_error.h>

#include <nx/network/socket.h>
#include <nx/network/socket_global.h>
#include <nx/metrics/metrics_storage.h>
#include <common/common_module.h>

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
    mutable QnMutex mutex;
    QnMutex connectionMtx;
    std::atomic<int> newPort;
    QHostAddress serverAddress;
    std::atomic<int> localPort;
    bool useSSL;
    int maxConnections;
    bool ddosWarned;
    SystemError::ErrorCode lastError;

    static QByteArray defaultPage;
    static QString pathIgnorePrefix;

    QnTcpListenerPrivate():
        newPort(0),
        localPort(0),
        useSSL(false),
        maxConnections(0),
        ddosWarned(false),
        lastError(SystemError::noError)
    {
    }
};

QByteArray QnTcpListenerPrivate::defaultPage;
QString QnTcpListenerPrivate::pathIgnorePrefix;


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

    QList<QByteArray> data = authorizationIter->second.split(' ');
    bool rez = false;
    if (data[0].toLower() == "basic" && data.size() > 1)
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
    QnCommonModule* commonModule,
    const QHostAddress& address,
    int port,
    int maxConnections,
    bool useSSL)
:
    QnCommonModuleAware(nullptr, true), //< Allow to create listeners without common module.
    d_ptr(new QnTcpListenerPrivate())
{
    if (commonModule)
        initializeContext(commonModule);

    Q_D(QnTcpListener);
    d->serverAddress = address;
    d->localPort = port;
    d->maxConnections = maxConnections;
    d->useSSL = useSSL;
}

QnTcpListener::~QnTcpListener()
{
    Q_D(QnTcpListener);
    stop();

    NX_ASSERT(!d->serverSocket);

    delete d_ptr;
}

static const int kSocketAcceptTimeoutMs = 250;

bool QnTcpListener::bindToLocalAddress()
{
    Q_D(QnTcpListener);

    const nx::network::SocketAddress localAddress(d->serverAddress.toString(), d->localPort);
    auto serverSocket = createAndPrepareSocket(d->useSSL, localAddress);
    if (!serverSocket
        || !serverSocket->setRecvTimeout(kSocketAcceptTimeoutMs))
    {
        const auto errorMessage = lm("Error: Unable to bind and listen on %1: %2")
            .args(localAddress, SystemError::toString(lastError()));

        NX_WARNING(this, errorMessage);
        qCritical() << errorMessage;
        return false;
    }
    d->serverSocket = std::move(serverSocket);

    {
        QnMutexLocker lk(&d->mutex);
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
    if (!serverSocket->setReuseAddrFlag(true) ||
        !serverSocket->setReusePortFlag(true) ||
        !serverSocket->bind(localAddress) ||
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
        QnMutexLocker lock(&d->connectionMtx);

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
    QnMutexLocker lock(&d->connectionMtx);
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

    QnMutexLocker lock(&d->connectionMtx);
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

    QList<QnLongRunnable*> oldConnections = d->connections;
    {
        QnMutexLocker lock(&d->connectionMtx);

        for (QList<QnLongRunnable*>::iterator itr = d->connections.begin();
            itr != d->connections.end(); ++itr)
        {
            QnLongRunnable* processor = *itr;
            processor->pleaseStop();
        }
        d->connections.clear();
    }

    for (QList<QnLongRunnable*>::iterator itr = oldConnections.begin();
        itr != oldConnections.end(); ++itr)
    {
        QnLongRunnable* processor = *itr;
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
                if (commonModule())
                {
                    commonModule()->metrics()->tcpConnections().total()++;
                    clientSocket->setBeforeDestroyCallback(
                        [metrics = commonModule()->metrics()]()
                        {
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
            }
        }
    }
    catch (const std::exception& e)
    {
        NX_WARNING(this, "Exception in TCPListener (%1:%2). %3",
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

    if (d->connections.size() > d->maxConnections)
    {
        if (!d->ddosWarned)
        {
            qWarning() << "Amount of TCP connections reached"
                << d->connections.size() << "of" << d->maxConnections
                << "Possible ddos attack! Reject incoming TCP connection";
            d->ddosWarned = true;
        }
        return;
    }
    d->ddosWarned = false;
    NX_VERBOSE(this, "New client connection from %1", socket->getForeignAddress().address);

    socket->setRecvTimeout(kDefaultSocketTimeout);
    socket->setSendTimeout(kDefaultSocketTimeout);
    QnTCPConnectionProcessor* processor = createRequestProcessor(std::move(socket));


    QnMutexLocker lock(&d->connectionMtx);
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
    QnMutexLocker lk(&d->mutex);
    return d->localEndpoint;
}

void QnTcpListener::setDefaultPage(const QByteArray& path)
{
    QnTcpListenerPrivate::defaultPage = path;
}

QByteArray QnTcpListener::defaultPage()
{
    return QnTcpListenerPrivate::defaultPage;
}

void QnTcpListener::setPathIgnorePrefix(const QString& path)
{
    QnTcpListenerPrivate::pathIgnorePrefix = path;
}

QString QnTcpListener::normalizedPath(const QString& path)
{
    int startIndex = 0;
    while (startIndex < path.length() && path[startIndex] == L'/')
        ++startIndex;

    int endIndex = path.size(); // [startIndex..endIndex)
    while (endIndex > 0 && path[endIndex-1] == L'/')
        --endIndex;

    if (path.mid(startIndex).startsWith(QnTcpListenerPrivate::pathIgnorePrefix))
        startIndex += QnTcpListenerPrivate::pathIgnorePrefix.length();
    return path.mid(startIndex, endIndex - startIndex);
}
