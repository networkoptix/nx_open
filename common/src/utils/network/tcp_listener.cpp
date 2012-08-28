#include "tcp_listener.h"
#include "socket.h"
#include "utils/common/log.h"
#include "tcp_connection_processor.h"
#include "ssl.h"

// ------------------------ QnRtspListenerPrivate ---------------------------

class QnTcpListener::QnTcpListenerPrivate
{
public:
    QnTcpListenerPrivate() {
        serverSocket = 0;
        newPort = 0;
        method = 0;
        ctx = 0;
    }
    TCPServerSocket* serverSocket;
    QMap<TCPSocket*, QnLongRunnable*> connections;
    QByteArray authDigest;
    QMutex portMutex;
    int newPort;
    QHostAddress serverAddress;
    const SSL_METHOD *method;
    SSL_CTX *ctx;
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


QnTcpListener::QnTcpListener(const QHostAddress& address, int port):
    d_ptr(new QnTcpListenerPrivate())
{
    Q_D(QnTcpListener);
    try {
        d->serverAddress = address;
        d->serverSocket = new TCPServerSocket(address.toString(), port, 5 ,true);
        start();
        cl_log.log("Server started at ", address.toString() + QLatin1String(":") + QString::number(port), cl_logINFO);
    }
    catch(const SocketException &e) {
        qCritical() << "Can't start TCP listener at address" << address << ":" << port << ". Reason: " << e.what();
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

void QnTcpListener::removeDisconnectedConnections()
{
    Q_D(QnTcpListener);
    for (QMap<TCPSocket*, QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end();)
    {
        QnLongRunnable* processor = itr.value();
        if (!processor->isRunning()) {
            delete processor;
            itr = d->connections.erase(itr);
        }
        else 
            ++itr;
    }
}

void QnTcpListener::removeAllConnections()
{
    Q_D(QnTcpListener);

    for (QMap<TCPSocket*, QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
    {
        QnLongRunnable* processor = itr.value();
        processor->pleaseStop();
    }

    for (QMap<TCPSocket*, QnLongRunnable*>::iterator itr = d->connections.begin(); itr != d->connections.end(); ++itr)
    {
        QnLongRunnable* processor = itr.value();
        delete processor;
    }
    d->connections.clear();
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
    Q_D(QnTcpListener);
    if (!d->serverSocket)
        m_needStop = true;
    while (!m_needStop)
    {
        if (d->newPort)
        {
            QMutexLocker lock(&d->portMutex);
            removeAllConnections();
            delete d->serverSocket;
            d->serverSocket = new TCPServerSocket(d->serverAddress.toString(), d->newPort);
            d->newPort = 0;
        }

        TCPSocket* clientSocket = d->serverSocket->accept();
        if (clientSocket) {
            qDebug() << "New client connection from " << clientSocket->getPeerAddress() << ':' << clientSocket->getForeignPort();
            QnTCPConnectionProcessor* processor = createRequestProcessor(clientSocket, this);
            clientSocket->setReadTimeOut(processor->getSocketTimeout());
            clientSocket->setWriteTimeOut(processor->getSocketTimeout());

            d->connections[clientSocket] = processor;
            processor->start();
        }
        removeDisconnectedConnections();
    }
}

void* QnTcpListener::getOpenSSLContext()
{
    Q_D(QnTcpListener);
    return d->ctx;
}
