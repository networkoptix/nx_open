#include "tcp_connection_processor.h"

#include <QtCore/QTime>

#include <utils/common/log.h>
#include <utils/network/http/httptypes.h>

#include "tcp_listener.h"
#include "tcp_connection_priv.h"
#include "err.h"

#ifndef Q_OS_WIN
#   include <netinet/tcp.h>
#endif

static const int MAX_REQUEST_SIZE = 1024*1024*15;


QnTCPConnectionProcessor::QnTCPConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket):
    d_ptr(new QnTCPConnectionProcessorPrivate)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    d->chunkedMode = false;
}

QnTCPConnectionProcessor::QnTCPConnectionProcessor(QnTCPConnectionProcessorPrivate* dptr, QSharedPointer<AbstractStreamSocket> socket):
    d_ptr(dptr)
{
    Q_D(QnTCPConnectionProcessor);
    d->socket = socket;
    //d->socket->setNoDelay(true);
    d->chunkedMode = false;
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
//    qDebug() << "Client request from " << d->socket->getPeerAddress().address.toString();
//    qDebug() << d->clientRequest;

    d->request = nx_http::Request();
    if( !d->request.parse( d->clientRequest ) )
    {
        qWarning() << Q_FUNC_INFO << "Invalid request format.";
        return;
    }
    QList<QByteArray> versionParts = d->request.requestLine.version.split('/');
    d->protocol = versionParts[0];
    d->requestBody = d->request.messageBody;
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
bool QnTCPConnectionProcessor::sendBuffer(const QnByteArray& sendBuffer)
{
    Q_D(QnTCPConnectionProcessor);

    QMutexLocker lock(&d->sockMutex);
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendBuffer(const QByteArray& sendBuffer)
{
    Q_D(QnTCPConnectionProcessor);

    QMutexLocker lock(&d->sockMutex);
    return sendData(sendBuffer.data(), sendBuffer.size());
}

bool QnTCPConnectionProcessor::sendData(const char* data, int size)
{
    Q_D(QnTCPConnectionProcessor);
    while (!needToStop() && size > 0 && d->socket->isConnected())
    {
        int sended = 0;
        sended = d->socket->send(data, size);
        if( sended <= 0 )
            break;

#ifdef DEBUG_RTSP
        dumpRtspData(data, sended);
#endif
        data += sended;
        size -= sended;
    }
    return d->socket->isConnected();
}


QString QnTCPConnectionProcessor::extractPath() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url.path();
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

void QnTCPConnectionProcessor::sendResponse(const QByteArray& transport, int code, const QByteArray& contentType, const QByteArray& contentEncoding, bool displayDebug)
{
    Q_D(QnTCPConnectionProcessor);

    d->response.statusLine.version = d->request.requestLine.version;
    d->response.statusLine.statusCode = code;
    d->response.statusLine.reasonPhrase = nx_http::StatusCode::toString((nx_http::StatusCode::Value)code);
    if (d->chunkedMode)
        d->response.headers.insert( nx_http::HttpHeader( "Transfer-Encoding", "chunked" ) );

    if (!contentEncoding.isEmpty())
        d->response.headers.insert( nx_http::HttpHeader( "Content-Encoding", contentEncoding ) );
    if (!contentType.isEmpty())
        d->response.headers.insert( nx_http::HttpHeader( "Content-Type", contentType ) );
    if (!d->chunkedMode)
        d->response.headers.insert( nx_http::HttpHeader( "Content-Length", QByteArray::number(d->responseBody.length()) ) );

    QByteArray response = d->response.toString();
    response.replace(0,4,transport);    //TODO #ak: looks too bad. Add support for any protocol to nx_http (nx_http has to be renamed in this case)
    if (!d->responseBody.isEmpty())
    {
        response += d->responseBody;
    }

    if (displayDebug)
        NX_LOG(lit("Server response to %1:\n%2").arg(d->socket->getPeerAddress().address.toString()).arg(QString::fromLatin1(response)), cl_logDEBUG1);

    NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Sending response to %1:\n%2-------------------\n\n\n").
        arg(d->socket->getPeerAddress().toString()).
        arg(QString::fromLatin1(response)), cl_logDEBUG1 );

    QMutexLocker lock(&d->sockMutex);
    sendData(response.data(), response.size());
}

bool QnTCPConnectionProcessor::sendChunk( const QnByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk( const QByteArray& chunk )
{
    return sendChunk( chunk.data(), chunk.size() );
}

bool QnTCPConnectionProcessor::sendChunk( const char* data, int size )
{
    Q_D(QnTCPConnectionProcessor);
    QByteArray result = QByteArray::number(size,16);
    result.append("\r\n");
    result.append(data, size);  //TODO/IMPL avoid copying by implementing writev in socket
    result.append("\r\n");

    int sended;
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
    case CODE_UNSPOORTED_TRANSPORT:
        return tr("Unsupported Transport");
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

QSharedPointer<AbstractStreamSocket> QnTCPConnectionProcessor::socket() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->socket;
}

int QnTCPConnectionProcessor::getSocketTimeout()
{
    Q_D(QnTCPConnectionProcessor);
    return d->socketTimeout;
}

int QnTCPConnectionProcessor::readSocket( quint8* buffer, int bufSize )
{
    Q_D(QnTCPConnectionProcessor);
    return d->socket->recv( buffer, bufSize );
}

bool QnTCPConnectionProcessor::readRequest()
{
    Q_D(QnTCPConnectionProcessor);

    //QElapsedTimer globalTimeout;
    d->request = nx_http::Request();
    d->response = nx_http::Response();
    d->clientRequest.clear();
    d->requestBody.clear();
    d->responseBody.clear();

    while (!needToStop() && d->socket->isConnected())
    {
        int readed;
        readed = d->socket->recv(d->tcpReadBuffer, TCP_READ_BUFFER_SIZE);
        if (readed > 0) 
        {
            //globalTimeout.restart();
            d->clientRequest.append((const char*) d->tcpReadBuffer, readed);
            if (isFullMessage(d->clientRequest))
            {
                NX_LOG( QnLog::HTTP_LOG_INDEX, QString::fromLatin1("Received request from %1:\n%2-------------------\n\n\n").
                    arg(d->socket->getPeerAddress().toString()).
                    arg(QString::fromLatin1(d->clientRequest)), cl_logDEBUG1 );
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
    other.d_ptr->protocol = d->protocol;
}

QUrl QnTCPConnectionProcessor::getDecodedUrl() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->request.requestLine.url;
}

void QnTCPConnectionProcessor::execute(QMutex& mutex)
{
    m_needStop = false;
    mutex.unlock();
    run();
    mutex.lock();
}

SocketAddress QnTCPConnectionProcessor::remoteHostAddress() const
{
    Q_D(const QnTCPConnectionProcessor);
    return d->socket ? d->socket->getPeerAddress() : SocketAddress();
}

void QnTCPConnectionProcessor::releaseSocket()
{
    Q_D(QnTCPConnectionProcessor);
    d->socket.clear();    
}
