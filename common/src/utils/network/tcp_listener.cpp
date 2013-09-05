#include "tcp_listener.h"
#include "socket.h"
#include "utils/common/log.h"
#include "tcp_connection_processor.h"
#include <openssl/ssl.h>

#include <utils/common/log.h>
#include <utils/common/systemerror.h>


// ------------------------ QnRtspListenerPrivate ---------------------------

class QnTcpListenerPrivate
{
public:
    QnTcpListenerPrivate() {
        serverSocket = 0;
        newPort = 0;
        method = 0;
        localPort = 0;
        ctx = 0;
    }
    AbstractStreamServerSocket* serverSocket;
    QList<QnLongRunnable*> connections;
    QByteArray authDigest;
    mutable QMutex portMutex;
    QMutex connectionMtx;
    int newPort;
    QHostAddress serverAddress;
    int localPort;
    const SSL_METHOD *method;
    SSL_CTX *ctx;
    int maxConnections;
    bool ddosWarned;
};

// ------------------------ QnRtspListener ---------------------------

bool QnTcpListener::authenticate(const QHttpRequestHeader& headers, QHttpResponseHeader& responseHeaders) const
{
    Q_D(const QnTcpListener);
    if (d->authDigest.isEmpty())
        return true;
    QList<QByteArray> data = headers.value(QLatin1String("Authorization")).toUtf8().split(' ');
    bool rez = false;
    if (data[0].toLower() == "basic" && data.size() > 1)
        rez = data[1] == d->authDigest;
    if (!rez) {
        responseHeaders.addValue(QLatin1String("WWW-Authenticate"), QLatin1String("Basic realm=\"Secure Area\""));
    }
    return rez;
}

void QnTcpListener::setAuth(const QByteArray& userName, const QByteArray& password)
{
    Q_D(QnTcpListener);
    QByteArray digest = userName + QByteArray(":") + password;
    d->authDigest = digest.toBase64();
}

QnTcpListener::QnTcpListener(const QHostAddress& address, int port, int maxConnections):
    d_ptr(new QnTcpListenerPrivate())
{
    Q_D(QnTcpListener);
    d->serverAddress = address;
    d->localPort = port;
    d->serverSocket = SocketFactory::createStreamServerSocket();
    //d->serverSocket = new TCPServerSocket(address.toString(), port, 5, true);
    d->maxConnections = maxConnections;
    d->ddosWarned = false;
    if( !d->serverSocket->setReuseAddrFlag(true) ||
        !d->serverSocket->bind(SocketAddress(address.toString(), port)) ||
        !d->serverSocket->listen() )
    //if( d->serverSocket->failed() )
    {
        const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();

        NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Initial bind failed: %3 (%4)").arg(d->serverAddress.toString()).arg(d->localPort).
            arg(prevErrorCode).arg(SystemError::toString(prevErrorCode)), cl_logWARNING );
        qCritical() << "Can't start TCP listener at address" << address << ":" << port << ". "
            "Reason: " << SystemError::toString(prevErrorCode) << "("<<prevErrorCode<<")";
    }
    else {
        cl_log.log("Server started at ", address.toString() + QLatin1String(":") + QString::number(port), cl_logINFO);
    }
}

QnTcpListener::~QnTcpListener()
{
    Q_D(QnTcpListener);
    stop();

    d->serverSocket->close();
    delete d->serverSocket;
    delete d_ptr;
}

void QnTcpListener::doPeriodicTasks()
{
    removeDisconnectedConnections();
}

void QnTcpListener::removeDisconnectedConnections()
{
    Q_D(QnTcpListener);

    QVector<QnLongRunnable*> toDeleteList;

    {
        QMutexLocker lock(&d->connectionMtx);
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

    foreach(QnLongRunnable* processor, toDeleteList)
        delete processor;
}

void QnTcpListener::removeOwnership(QnLongRunnable* processor)
{
    Q_D(QnTcpListener);
    QMutexLocker lock(&d->connectionMtx);
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

    QMutexLocker lock(&d->connectionMtx);
    d->connections << processor;
}


void QnTcpListener::pleaseStop()
{
    QnLongRunnable::pleaseStop();

    Q_D(QnTcpListener);
    qWarning() << "QnTcpListener::pleaseStop() called";
    d->serverSocket->close();
}

void QnTcpListener::removeAllConnections()
{
    Q_D(QnTcpListener);

    QList<QnLongRunnable*> oldConnections = d->connections;
    {
        QMutexLocker lock(&d->connectionMtx);

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
        NX_LOG( QString::fromLatin1("TCPListener. Stopping processor (sysThreadID %1)").arg(processor->sysThreadID()), cl_logWARNING );
        delete processor;
    }
}

void QnTcpListener::updatePort(int newPort)
{
    Q_D(QnTcpListener);
    QMutexLocker lock(&d->portMutex);
    d->newPort = newPort;
}

bool QnTcpListener::enableSSLMode()
{
    Q_D(QnTcpListener);

    QFile f(QLatin1String(":/cert.pem"));
    if (!f.open(QIODevice::ReadOnly)) 
    {
        qWarning() << "No SSL sertificate for mediaServer!";
        return false;
    }
    QByteArray certData = f.readAll();

    SSL_library_init();
    OpenSSL_add_all_algorithms();   
    SSL_load_error_strings();     
    d->method = SSLv23_server_method();
    d->ctx = SSL_CTX_new(d->method);

    BIO *bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    X509 *x = PEM_read_bio_X509_AUX(bufio, NULL, d->ctx->default_passwd_callback, d->ctx->default_passwd_callback_userdata);
    SSL_CTX_use_certificate(d->ctx, x);
    BIO_free(bufio);

    bufio = BIO_new_mem_buf((void*) certData.data(), certData.size());
    EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bufio, NULL, d->ctx->default_passwd_callback, d->ctx->default_passwd_callback_userdata);
    SSL_CTX_use_PrivateKey(d->ctx, pkey);
    BIO_free(bufio);

    return true;
}

void QnTcpListener::run()
{
    saveSysThreadID();

    Q_D(QnTcpListener);

    NX_LOG( QString::fromLatin1("Entered QnTcpListener::run. %1:%2, system thread id %3").arg(d->serverAddress.toString()).arg(d->localPort).arg(sysThreadID()), cl_logWARNING );
    try
    {
        if (!d->serverSocket)
            m_needStop = true;
        while (!needToStop())
        {
            if (d->newPort)
            {
                QMutexLocker lock(&d->portMutex);
                NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Switching port to: %3").arg(d->serverAddress.toString()).arg(d->localPort).arg(d->newPort), cl_logWARNING );
                removeAllConnections();
                delete d->serverSocket;
                //d->serverSocket = new TCPServerSocket(d->serverAddress.toString(), d->newPort);
                //if( d->serverSocket->failed() )
                d->serverSocket = SocketFactory::createStreamServerSocket();
                if( !d->serverSocket->setReuseAddrFlag(true) ||
                    !d->serverSocket->bind(SocketAddress(d->serverAddress.toString(), d->newPort)) ||
                    !d->serverSocket->listen() )
                {
                    const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                    NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Bind failed: %3").arg(d->serverAddress.toString()).arg(d->localPort).
                        arg(SystemError::toString(prevErrorCode)), cl_logWARNING );
                    QThread::msleep(1000);
                    continue;
                }
                NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Switched to port %3").arg(d->serverAddress.toString()).arg(d->localPort).arg(d->newPort), cl_logWARNING );

                d->localPort = d->newPort;
                d->newPort = 0;
            }

            AbstractStreamSocket* clientSocket = d->serverSocket->accept();
//            delete clientSocket;
//            clientSocket = NULL;
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
                NX_LOG( QString::fromLatin1("New client connection from %1").arg(clientSocket->getForeignAddress().address.toString()), cl_logDEBUG1 );
                QnTCPConnectionProcessor* processor = createRequestProcessor(clientSocket, this);
                clientSocket->setRecvTimeout(processor->getSocketTimeout());
                clientSocket->setSendTimeout(processor->getSocketTimeout());
                
                QMutexLocker lock(&d->connectionMtx);
                d->connections << processor;
                processor->start();
            }
            else
            {
                const SystemError::ErrorCode prevErrorCode = SystemError::getLastOSErrorCode();
                if( prevErrorCode != SystemError::timedOut ) {
                    NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Accept failed: %3 (%4)").arg(d->serverAddress.toString()).arg(d->localPort).
                        arg(prevErrorCode).arg(SystemError::toString(prevErrorCode)), cl_logWARNING );
                    QThread::msleep(1000);
                    d->newPort = d->localPort; // reopen tcp socket
                }
            }
            doPeriodicTasks();
        }
        NX_LOG( QString::fromLatin1("TCPListener (%1:%2). Removing all connections before stop").arg(d->serverAddress.toString()).arg(d->localPort), cl_logWARNING );
        removeAllConnections();
    }
    catch( const std::exception& e )
    {
        NX_LOG( QString::fromLatin1("Exception in TCPListener (%1:%2). %3").
            arg(d->serverAddress.toString()).arg(d->localPort).arg(QString::fromLatin1(e.what())), cl_logWARNING );
    }
    NX_LOG( QString::fromLatin1("Exiting QnTcpListener::run. %1:%2").arg(d->serverAddress.toString()).arg(d->localPort), cl_logWARNING );
}

SSL_CTX* QnTcpListener::getOpenSSLContext()
{
    Q_D(QnTcpListener);
    return d->ctx;
}

int QnTcpListener::getPort() const
{
    Q_D(const QnTcpListener);
    QMutexLocker lock(&d->portMutex);
    return d->serverSocket->getLocalAddress().port;
}
