#include "tcp_connection_processor.h"
#include "tcp_listener.h"
#include "tcp_connection_priv.h"

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
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* dptr, TCPSocket* socket, QnTcpListener* _owner):
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    //d->socket->setNoDelay(true);
    d->owner = _owner;
}


QnTCPConnectionProcessor::~QnTCPConnectionProcessor()
{
    stop();
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

void QnTCPConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    QMutexLocker lock(&d->sockMutex);
    while (!m_needStop && size > 0 && d->socket->isConnected())
    {
        int sended = d->socket->send(data, size);
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
    QString path = d->requestHeaders.path();
    int pos = path.indexOf(QLatin1String("://"));
    if (pos == -1)
        return QString();
    pos = path.indexOf(QLatin1Char('/'), pos+3);
    if (pos == -1)
        return QString();
    return path.mid(pos+1);
}

void QnTCPConnectionProcessor::sendResponse(const QByteArray& transport, int code, const QByteArray& contentType)
{
    Q_D(QnTCPConnectionProcessor);
    d->responseHeaders.setStatusLine(code, codeToMessage(code), d->requestHeaders.majorVersion(), d->requestHeaders.minorVersion());
    if (!d->responseBody.isEmpty())
    {
        d->responseHeaders.setContentLength(d->responseBody.length());
        //d->responseHeaders.setContentType("application/sdp");
        d->responseHeaders.setContentType(QLatin1String(contentType));
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
    d->socket->send(response.data(), response.size());
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

    while (!m_needStop && d->socket->isConnected())
    {
        int readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
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

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    QByteArray data = d->requestHeaders.path().toUtf8();
    data = data.replace("+", "%20");
    return QUrl::fromEncoded(data);

}
