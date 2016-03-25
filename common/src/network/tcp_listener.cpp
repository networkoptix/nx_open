#include <atomic>

#include "tcp_connection_processor.h"
#include "tcp_listener.h"

#include <nx/utils/log/log.h>
#include <utils/common/systemerror.h>

#include <nx/network/socket.h>
#include <nx/network/socket_global.h>

// ------------------------ QnTcpListenerPrivate ---------------------------

class QnTcpListenerPrivate
{
public:
    nx::network::SocketGlobals::InitGuard socketGlobalsInitGuard;
    AbstractStreamServerSocket* serverSocket;
    QList<QnLongRunnable*> connections;
    QByteArray authDigest;
    QnMutex connectionMtx;
    std::atomic<int> newPort;
    QHostAddress serverAddress;
    std::atomic<int> localPort;
    bool useSSL;
    int maxConnections;
    bool ddosWarned;

    QnTcpListenerPrivate()
    :
        serverSocket(nullptr),
        newPort(0),
        localPort(0),
        useSSL(false),
        maxConnections(0),
        ddosWarned(false)
    {
    }
};

// ------------------------ QnTcpListener ---------------------------

bool QnTcpListener::authenticate(const nx_http::Request& request, nx_http::Response& response) const
{
    Q_D(const QnTcpListener);
    if (d->authDigest.isEmpty())
        return true;
    nx_http::HttpHeaders::const_iterator authorizationIter = request.headers.find( "Authorization" );
    if( authorizationIter == request.headers.end() )
        return false;

    QList<QByteArray> data = authorizationIter->second.split(' ');
    bool rez = false;
    if (data[0].toLower() == "basic" && data.size() > 1)
        rez = data[1] == d->authDigest;
    if (!rez)
        nx_http::insertOrReplaceHeader( &response.headers, std::make_pair("WWW-Authenticate", "Basic realm=\"Secure Area\"") );
    return rez;
}

void QnTcpListener::setAuth(const QByteArray& userName, const QByteArray& password)
{
    Q_D(QnTcpListener);
    QByteArray digest = userName + QByteArray(":") + password;
    d->authDigest = digest.toBase64();
}

QnTcpListener::QnTcpListener( const QHostAddress& address, int port, int maxConnections, bool useSSL )
:
    d_ptr(new QnTcpListenerPrivate())
{
    Q_D(QnTcpListener);
    d->serverAddress = address;
    d->localPort = port;
    d->serverSocket = nullptr;
    d->maxConnections = maxConnections;
    d->useSSL = useSSL;
}

QnTcpListener::~QnTcpListener()
{
    Q_D(QnTcpListener);
    stop();

    NX_ASSERT(d->serverSocket == nullptr);

    delete d_ptr;
}

static const int kSocketAcceptTimeoutMs = 250;

bool QnTcpListener::bindToLocalAddress()
{
    Q_D(QnTcpListener);

    const SocketAddress localAddress(d->serverAddress.toString(), d->localPort);
    d->serverSocket = createAndPrepareSocket(
        d->useSSL,
        localAddress);
    if (!d->serverSocket ||
        !d->serverSocket->setRecvTimeout(kSocketAcceptTimeoutMs))
    {
        const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
        NX_LOG(lit("TCPListener (%1:%2). Initial bind failed: %3 (%4)").arg(d->serverAddress.toString()).arg(d->localPort).
            arg(prevErrorCode).arg(SystemError::toString(prevErrorCode)), cl_logWARNING);
        qCritical() << "Can't start TCP listener at address" << d->serverAddress << ":" << d->localPort << ". "
            "Reason: " << SystemError::toString(prevErrorCode) << "(" << prevErrorCode << ")";
        return false;
    }

    NX_LOG(lit("Server started at %1").arg(localAddress.toString()), cl_logINFO);
    return true;
}

void QnTcpListener::doPeriodicTasks()
{
    removeDisconnectedConnections();
}

AbstractStreamServerSocket* QnTcpListener::createAndPrepareSocket(
    bool sslNeeded,
    const SocketAddress& localAddress)
{
    auto serverSocket = SocketFactory::createStreamServerSocket(sslNeeded);
    if (!serverSocket->setReuseAddrFlag(true) ||
        !serverSocket->bind(localAddress) ||
        !serverSocket->listen())
    {
        return nullptr;
    }

    return serverSocket.release();
}

void QnTcpListener::destroyServerSocket(AbstractStreamServerSocket* serverSocket)
{
    delete serverSocket;
}

void QnTcpListener::removeDisconnectedConnections()
{
    Q_D(QnTcpListener);

    QVector<QnLongRunnable*> toDeleteList;

    {
        QnMutexLocker lock( &d->connectionMtx );
        for (QList<QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end();)
        {
            QnLongRunnable* processor = *itr;
            if (!processor->isRunning()) {
                toDeleteList << processor;
                itr = d->connections.erase(itr);
            }
            else 
                ++itr;
        }
    }

    for(QnLongRunnable* processor: toDeleteList)
        delete processor;
}

void QnTcpListener::removeOwnership(QnLongRunnable* processor)
{
    Q_D(QnTcpListener);
    QnMutexLocker lock( &d->connectionMtx );
    for (QList<QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
    {
        if (processor == *itr) {
            d->connections.erase(itr);
            break;
        }
    }
}

void QnTcpListener::addOwnership(QnLongRunnable* processor)
{
    Q_D(QnTcpListener);

    QnMutexLocker lock( &d->connectionMtx );
    d->connections << processor;
}

bool QnTcpListener::isSslEnabled() const
{
    Q_D(const QnTcpListener);
    return d->useSSL;
}

void QnTcpListener::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    Q_D(QnTcpListener);
    qWarning() << "QnTcpListener::pleaseStop() called";
}

void QnTcpListener::removeAllConnections()
{
    Q_D(QnTcpListener);

    QList<QnLongRunnable*> oldConnections = d->connections;
    {
        QnMutexLocker lock( &d->connectionMtx );

        for (QList<QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
        {
            QnLongRunnable* processor = *itr;
            processor->pleaseStop();
        }
        d->connections.clear();
    }

    for (QList<QnLongRunnable*>::iterator itr = oldConnections.begin(); itr != oldConnections.end(); ++itr)
    {
        QnLongRunnable* processor = *itr;
        NX_LOG( lit("TCPListener. Stopping processor (sysThreadID %1)").arg(processor->systemThreadId()), cl_logWARNING );
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
    if( !d->serverSocket )
        bindToLocalAddress();

    NX_LOG( lit("Entered QnTcpListener::run. %1:%2, system thread id %3").arg(d->serverAddress.toString()).arg(d->localPort).arg(systemThreadId()), cl_logINFO );
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
                NX_LOG( lit("TCPListener (%1:%2). Switching port to: %3").arg(d->serverAddress.toString()).arg(oldPort).arg(d->localPort), cl_logINFO );
                //removeAllConnections();
                destroyServerSocket(d->serverSocket);
                d->serverSocket = nullptr;
                if( !bindToLocalAddress() )
                {
                    QThread::msleep(1000);
                    continue;
                }
                NX_LOG( lit("TCPListener (%1:%2). Switched to port %3").arg(d->serverAddress.toString()).arg(oldPort).arg(d->localPort), cl_logINFO );
                int currentValue = d->localPort;
                d->newPort.compare_exchange_strong(currentValue, 0);  // reset newPort if no more changes
            }

            if (d->localPort == 0 && d->serverSocket)
                d->localPort = d->serverSocket->getLocalAddress().port;


            AbstractStreamSocket* clientSocket = d->serverSocket->accept();
            if( clientSocket )
            {
                if (d->connections.size() > d->maxConnections)
                {
                    if (!d->ddosWarned) {
                        qWarning() << "Too many TCP connections. Possible ddos attack! Ignore connection";
                        d->ddosWarned = true;
                    }
                    delete clientSocket;
                    continue;
                }
                d->ddosWarned = false;
                NX_LOG( lit("New client connection from %1").arg(clientSocket->getForeignAddress().address.toString()), cl_logDEBUG1 );
                QnTCPConnectionProcessor* processor = createRequestProcessor(QSharedPointer<AbstractStreamSocket>(clientSocket));
                clientSocket->setRecvTimeout(processor->getSocketTimeout());
                clientSocket->setSendTimeout(processor->getSocketTimeout());
                
                QnMutexLocker lock( &d->connectionMtx );
                d->connections << processor;
                processor->start();
            }
            else
            {
                const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                if( prevErrorCode != SystemError::timedOut &&
                    prevErrorCode != SystemError::again &&
                    prevErrorCode != SystemError::interrupted )
                {
                    NX_LOG( lit("TCPListener (%1:%2). Accept failed: %3 (%4)").arg(d->serverAddress.toString()).arg(d->localPort).
                        arg(prevErrorCode).arg(SystemError::toString(prevErrorCode)), cl_logWARNING );
                    QThread::msleep(1000);
                    int zerro = 0;
                    d->newPort.compare_exchange_strong(zerro, d->localPort);  // reopen tcp socket
                }
            }
            doPeriodicTasks();
        }
        NX_LOG( lit("TCPListener (%1:%2). Removing all connections before stop").arg(d->serverAddress.toString()).arg(d->localPort), cl_logWARNING );
        removeAllConnections();

        destroyServerSocket(d->serverSocket);
        d->serverSocket = nullptr;
    }
    catch( const std::exception& e )
    {
        NX_LOG( lit("Exception in TCPListener (%1:%2). %3").
            arg(d->serverAddress.toString()).arg(d->localPort).arg(QString::fromLatin1(e.what())), cl_logWARNING );
    }
    NX_LOG( lit("Exiting QnTcpListener::run. %1:%2").arg(d->serverAddress.toString()).arg(d->localPort), cl_logWARNING );
}

int QnTcpListener::getPort() const
{
    Q_D(const QnTcpListener);
    return d->localPort;
}

static QByteArray m_defaultPage;

void QnTcpListener::setDefaultPage(const QByteArray& path)
{
    m_defaultPage = path;
}

QByteArray QnTcpListener::defaultPage()
{
    return m_defaultPage;
}
