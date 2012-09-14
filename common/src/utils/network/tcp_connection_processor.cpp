#include "tcp_connection_processor.h"
#include "tcp_listener.h"
#include "tcp_connection_priv.h"
#include "ssl.h"
#include "err.h"

#ifndef Q_OS_WIN
#include <netinet/tcp.h>
#endif

static const int MAX_REQUEST_SIZE = 1024*1024*15;

QnTCPConnectionProcessor::QnTCPConnectionProcessor(TCPSocket* socket, QnTcpListener* _owner):
    d_ptr(new QnTCPConnectionProcessorPrivate)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    d->owner = _owner;
    d->chunkedMode = false;
    d->ssl = 0;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* dptr, TCPSocket* socket, QnTcpListener* _owner):
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    //d->socket->setNoDelay(true);
    d->owner = _owner;
    d->chunkedMode = false;
    d->ssl = 0;
}


QnTCPConnectionProcessor::~QnTCPConnectionProcessor()
{
    stop();
    if (d_ptr->ssl)
        SSL_free(d_ptr->ssl);
    delete d_ptr;
}

int QnTCPConnectionProcessor::isFullMessage(const QByteArray& message)
{
    QByteArray lRequest = message.toLower();
    QByteArray delimiter = "\n";
    int pos = lRequest.indexOf(delimiter);
    if (pos == -1)
        return 0;
    if (pos > 0 && lRequest[pos-1] == '\r')
        delimiter = "\r\n";
    int contentLen = 0;
    int contentLenPos = lRequest.indexOf("content-length");
    if (contentLenPos >= 0)
    {
        int posStart = -1;
        int posEnd = -1;
        for (int i = contentLenPos+1; i < lRequest.length(); ++i)
        {
            if (lRequest[i] >= '0' && lRequest[i] <= '9')
            {
                if (posStart == -1)
                    posStart = i;
                posEnd = i;
            }
            else if (lRequest[i] == '\n' || lRequest[i] == '\r')
                break;
        }
        if (posStart >= 0)
            contentLen = lRequest.mid(posStart, posEnd - posStart+1).toInt();
    }
    QByteArray dblDelim = delimiter + delimiter;
    //return lRequest.endsWith(dblDelim) && (!contentLen || lRequest.indexOf(dblDelim) < lRequest.length()-dblDelim.length());
    if (lRequest.indexOf(dblDelim) == -1)
        return 0;
    int expectedSize = lRequest.indexOf(dblDelim) + dblDelim.size() + contentLen;
    if (expectedSize > lRequest.size())
        return 0;
    else 
        return expectedSize;
}

void QnTCPConnectionProcessor::parseRequest()
{
    Q_D(QnTCPConnectionProcessor);
    qDebug() << "Client request from " << d->socket->getPeerAddress();
    qDebug() << d->clientRequest;

    QList<QByteArray> lines = d->clientRequest.split('\n');
    bool firstLine = true;
    foreach (const QByteArray& l, lines)
    {
        QByteArray line = l.trimmed();
        if (line.isEmpty())
            break;
        if (firstLine)
        {
            QList<QByteArray> params = line.split(' ');
            if (params.size() != 3)
            {
                qWarning() << Q_FUNC_INFO << "Invalid request format.";
                return;
            }
            QList<QByteArray> version = params[2].split('/');
            d->protocol = version[0];
            int major = 1;
            int minor = 0;
            if (version.size() > 1)
            {
                QList<QByteArray> v = version[1].split('.');
                major = v[0].toInt();
                if (v.length() > 1)
                    minor = v[1].toInt();
            }
            d->requestHeaders = QHttpRequestHeader(QLatin1String(params[0]), QLatin1String(params[1]), major, minor);
            firstLine = false;
        }
        else
        {
            QList<QByteArray> params = line.split(':');
            if (params.size() > 1)
                d->requestHeaders.addValue(QLatin1String(params[0].trimmed()), QLatin1String(params[1].trimmed()));
        }
    }
    QByteArray delimiter = "\n";
    int pos = d->clientRequest.indexOf(delimiter);
    if (pos == -1)
        return;
    if (pos > 0 && d->clientRequest[pos-1] == '\r')
        delimiter = "\r\n";
    QByteArray dblDelim = delimiter + delimiter;
    int bodyStart = d->clientRequest.indexOf(dblDelim);
    if (bodyStart >= 0 && d->requestHeaders.value(QLatin1String("content-length")).toInt() > 0)
        d->requestBody = d->clientRequest.mid(bodyStart + dblDelim.length());
}

/*
void QnTCPConnectionProcessor::bufferData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    d->sendBuffer.write(data, size);
}

QnByteArray& QnTCPConnectionProcessor::getSendBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    return d->sendBuffer;
}

void QnTCPConnectionProcessor::sendBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    sendData(d->sendBuffer.data(), d->sendBuffer.size());
}

void QnTCPConnectionProcessor::clearBuffer()
{
    Q_D(QnTCPConnectionProcessor);
    d->sendBuffer.clear();
}
*/
void QnTCPConnectionProcessor::sendBuffer(const QnByteArray& sendBuffer)
{
    //Q_D(QnTCPConnectionProcessor);
    sendData(sendBuffer.data(), sendBuffer.size());
}

void QnTCPConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    QMutexLocker lock(&d->sockMutex);
    while (!m_needStop && size > 0 && d->socket->isConnected())
    {
        int sended;
        if (d->ssl)
            sended = SSL_write(d->ssl, data, size);
        else
            sended = d->socket->send(data, size);
        if (sended > 0) {
#ifdef DEBUG_RTSP
            dumpRtspData(data, sended);
#endif
            data += sended;
            size -= sended;
        }
    }
}


QString QnTCPConnectionProcessor::extractPath() const
{
    Q_D(const QnTCPConnectionProcessor);
    return extractPath(d->requestHeaders.path());
}

QString QnTCPConnectionProcessor::extractPath(const QString& fullUrl)
{
    int pos = fullUrl.indexOf(QLatin1String("://"));
    if (pos == -1)
        return fullUrl;
    pos = fullUrl.indexOf(QLatin1Char('/'), pos+3);
    if (pos == -1)
        return QString();
    return fullUrl.mid(pos+1);
}

void QnTCPConnectionProcessor::sendResponse(const QByteArray& transport, int code, const QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);
    d->responseHeaders.setStatusLine(code, codeToMessage(code), d->requestHeaders.majorVersion(), d->requestHeaders.minorVersion());
    if (d->chunkedMode)
    {
        d->responseHeaders.setValue(QLatin1String("Transfer-Encoding"), QLatin1String("chunked"));
        d->responseHeaders.setContentType(QLatin1String(contentType));
    }
    if (!d->responseBody.isEmpty())
    {
        //d->responseHeaders.setContentLength(d->responseBody.length());
        //d->responseHeaders.setContentType(QLatin1String(contentType));
        d->responseHeaders.setValue(QLatin1String("Content-Type"), QLatin1String(contentType));
        d->responseHeaders.setValue(QLatin1String("Content-Length"), QString::number(d->responseBody.length()));
    }

    QByteArray response = d->responseHeaders.toString().toUtf8();
    response.replace(0,4,transport);
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
    }

    qDebug() << "Server response to " << d->socket->getPeerAddress();
    qDebug() << "\n" << response;

    QMutexLocker lock(&d->sockMutex);
    if (d->ssl)
        SSL_write(d->ssl, response.data(), response.size());
    else
        d->socket->send(response.data(), response.size());
}

bool QnTCPConnectionProcessor::sendChunk(const QnByteArray& chunk)
{
    Q_D(QnTCPConnectionProcessor);
    QByteArray result = QByteArray::number(chunk.size(),16);
    result.append("\r\n");
    result.append(chunk.data(), chunk.size());
    result.append("\r\n");

    int sended;
    if (d->ssl)
        sended = SSL_write(d->ssl, result.data(), result.size());
    else
        sended = d->socket->send(result);
    return sended == result.size();
}

QString QnTCPConnectionProcessor::codeToMessage(int code)
{
    switch(code)
    {
    case CODE_OK:
        return tr("OK");
    case CODE_NOT_FOUND:
        return tr("Not Found");
    case CODE_NOT_IMPLEMETED:
        return tr("Not Implemented");
    case CODE_INTERNAL_ERROR:
        return tr("Internal Server Error");
    case CODE_INVALID_PARAMETER:
        return tr("Invalid Parameter");
    }
    return QString ();
}

void QnTCPConnectionProcessor::pleaseStop()
{
    Q_D(QnTCPConnectionProcessor);
    if (d->socket)
        d->socket->close();
    QnLongRunnable::pleaseStop();
}

int QnTCPConnectionProcessor::getSocketTimeout()
{
    Q_D(QnTCPConnectionProcessor);
    return d->socketTimeout;
}

bool QnTCPConnectionProcessor::readRequest()
{
    Q_D(QnTCPConnectionProcessor);

    QTime globalTimeout;
    d->requestHeaders = QHttpRequestHeader();
    d->clientRequest.clear();
    d->responseHeaders = QHttpResponseHeader();
    d->requestBody.clear();
    d->responseBody.clear();

    if (d->owner->getOpenSSLContext() && !d->ssl)
    {
        d->ssl = SSL_new((SSL_CTX*) d->owner->getOpenSSLContext());  // get new SSL state with context 
        if (!SSL_set_fd(d->ssl, d->socket->handle()))    // set connection to SSL state 
            return false;
        if (SSL_accept(d->ssl) != 1) 
            return false; // ssl error
        
    }

    while (!m_needStop && d->socket->isConnected())
    {
        int readed;
        if (d->ssl) 
            readed = SSL_read(d->ssl, d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        else
            readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) 
        {
            globalTimeout.restart();
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage(d->clientRequest))
            {
                return true;
            }
            else if (d->clientRequest.size() > MAX_REQUEST_SIZE)
            {
                qWarning() << "Too large HTTP client request. Ignoring";
                return false;
            }
        }
        else //if (globalTimeout.elapsed() > CONNECTION_TIMEOUT)
        {
            break;
        }
    }
    return false;
}

void QnTCPConnectionProcessor::copyClientRequestTo(QnTCPConnectionProcessor& other)
{
    Q_D(const QnTCPConnectionProcessor);
    other.d_ptr->clientRequest = d->clientRequest;
    other.d_ptr->ssl = d->ssl;
}

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    QByteArray data = d->requestHeaders.path().toUtf8();
    data = data.replace("+", "%20");
    return QUrl::fromEncoded(data);

}

void QnTCPConnectionProcessor::execute(QMutex& mutex)
{
    m_needStop = false;
    mutex.unlock();
    run();
    mutex.lock();
}
