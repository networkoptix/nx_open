#include "tcp_connection_processor.h"
#include "tcp_listener.h"
#include "tcp_connection_priv.h"

#ifndef Q_OS_WIN
#include <netinet/tcp.h>
#endif

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

bool QnTCPConnectionProcessor::isFullMessage(const QByteArray& message)
{
    QByteArray lRequest = message.toLower();
    QByteArray delimiter = "\n";
    int pos = lRequest.indexOf(delimiter);
    if (pos == -1)
        return false;
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
    return lRequest.endsWith(dblDelim) && (!contentLen || lRequest.indexOf(dblDelim) < lRequest.length()-dblDelim.length());
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
            d->requestHeaders = QHttpRequestHeader(params[0], params[1], major, minor);
            firstLine = false;
        }
        else
        {
            QList<QByteArray> params = line.split(':');
            if (params.size() > 1)
                d->requestHeaders.addValue(params[0].trimmed(), params[1].trimmed());
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
    if (bodyStart >= 0 && d->requestHeaders.value("content-length").toInt() > 0)
        d->requestBody = d->clientRequest.mid(bodyStart + dblDelim.length());
}

void QnTCPConnectionProcessor::bufferData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    d->sendBuffer.write(data, size);
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
    int pos = path.indexOf("://");
    if (pos == -1)
        return QString();
    pos = path.indexOf('/', pos+3);
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
        d->responseHeaders.setContentType(contentType);
    }

    QByteArray response = d->responseHeaders.toString().toUtf8();
    response.replace(0,4,transport);
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
        response += ENDL;
    }

    //qDebug() << "Server response to " << d->socket->getPeerAddress();
    //qDebug() << "\n" << response;

    QMutexLocker lock(&d->sockMutex);
    d->socket->send(response.data(), response.size());
}

QString QnTCPConnectionProcessor::codeToMessage(int code)
{
    switch(code)
    {
    case CODE_OK:
        return "OK";
    case CODE_NOT_FOUND:
        return "Not Found";
    case CODE_NOT_IMPLEMETED:
        return "Not Implemented";
    case CODE_INTERNAL_ERROR:
        return "Internal Server Error";
    case CODE_INVALID_PARAMETER:
        return "Invalid Parameter";
    }
    return QString ();
}

void QnTCPConnectionProcessor::pleaseStop()
{
    Q_D(QnTCPConnectionProcessor);
    if (d->socket)
        d->socket->close();
    CLLongRunnable::pleaseStop();
}

int QnTCPConnectionProcessor::getSocketTimeout()
{
    Q_D(QnTCPConnectionProcessor);
    return d->socketTimeout;
}
