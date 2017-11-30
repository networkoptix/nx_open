#include "simple_http_client.h"

#include <sstream>

#include <QtCore/QCryptographicHash>
#include <QtCore/QUrl>

#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "../http/http_types.h"

//static const int MAX_LINE_LENGTH = 1024*16;

using namespace std;

QString toString( CLHttpStatus status )
{
    switch( status )
    {
        case CL_HTTP_SUCCESS:
            return lit("%1 (OK)").arg(status);
        case CL_HTTP_REDIRECT:
            return lit("%1 (REDIRECT)").arg(status);
        case CL_HTTP_BAD_REQUEST:
            return lit("%1 (BAD REQUEST)").arg(status);
        case CL_HTTP_AUTH_REQUIRED:
            return lit("%1 (AUTH REQUIRED)").arg(status);
        case CL_HTTP_NOT_FOUND:
            return lit("%1 (NOT FOUND)").arg(status);
        default:
            return lit("%1 (UNKNOWN)").arg(status);
    }
}


CLSimpleHTTPClient::CLSimpleHTTPClient(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_connected(false),
    m_timeout(timeout),
    m_auth(auth),
    m_dataRestPtr(0),
    m_dataRestLen(0),
    m_localPort(0)
{
    initSocket();
}

CLSimpleHTTPClient::CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host.toString()),
    m_port(port),
    m_connected(false),
    m_timeout(timeout),
    m_auth(auth),
    m_dataRestPtr(0),
    m_dataRestLen(0),
    m_localPort(0)
{
    initSocket();
}

CLSimpleHTTPClient::CLSimpleHTTPClient(const nx::utils::Url& url, unsigned int timeout, const QAuthenticator& auth):
    m_host(url.host()),
    m_port(url.port()),
    m_connected(false),
    m_timeout(timeout),
    m_auth(auth),
    m_dataRestPtr(0),
    m_dataRestLen(0),
    m_localPort(0)
{
    initSocket(url.scheme() == lit("https"));
}


void CLSimpleHTTPClient::initSocket(bool ssl)
{
    m_sock = TCPSocketPtr(SocketFactory::createStreamSocket(ssl).release());

    if( !m_sock->setRecvTimeout(m_timeout) || !m_sock->setSendTimeout(m_timeout) )
    {
        m_sock.clear();
        m_connected = false;
    }
}

CLSimpleHTTPClient::~CLSimpleHTTPClient()
{
}

QHostAddress CLSimpleHTTPClient::getLocalHost() const
{
    return !m_sock ? QHostAddress() : QHostAddress(m_sock->getLocalAddress().address.toString());
}

void CLSimpleHTTPClient::addHeader(const QByteArray& key, const QByteArray& value)
{
    m_additionHeaders[key] = value;
}

CLHttpStatus CLSimpleHTTPClient::doPOST(const QString& requestStr, const QString& body, bool recursive)
{
    return doPOST(requestStr.toUtf8(), body, recursive);
}

CLHttpStatus CLSimpleHTTPClient::doPOST(const QByteArray& requestStr, const QString& body, bool recursive)
{
    if (!m_sock)
        return CL_TRANSPORT_ERROR;

    try
    {
        if (!m_connected)
        {
            if (m_host.isEmpty())
                return CL_TRANSPORT_ERROR;

            NX_VERBOSE(this, lm("Connecting to %1:%2").arg(m_host).arg(m_port));
            if (!m_sock->connect(m_host, m_port))
            {
                const auto systemErrorCode = SystemError::getLastOSErrorCode();
                NX_VERBOSE(this, lm("Failed to connect. %1")
                    .arg(SystemError::toString(systemErrorCode)));
                return CL_TRANSPORT_ERROR;
            }

            NX_VERBOSE(this, lm("Connect succeeded"));
        }

        QByteArray request;
        request.append("POST ");
        if( !requestStr.startsWith('/') )
            request.append('/');
        QByteArray encodedRequest = QUrl(QLatin1String(requestStr)).toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
        request.append(encodedRequest);
        request.append(" HTTP/1.1\r\n");
        request.append("Host: ");
        request.append(m_host.toUtf8());
        request.append("\r\n");
        request.append("User-Agent: Simple HTTP Client 1.0\r\n");
        request.append("Accept: */*\r\n");

        addExtraHeaders(request);

        if (!request.contains("Content-Type"))
            request.append("Content-Type: application/x-www-form-urlencoded\r\n");

        if (m_auth.user().length()>0 && mNonce.isEmpty())
        {
            request.append(basicAuth());
            request.append("\r\n");
        }
        else if (m_auth.password().length()>0 && !mNonce.isEmpty())
        {
            request.append(digestAccess(lit("POST"), QLatin1String(requestStr)));
        }

        request.append("Content-Length: ");
        request.append(QByteArray::number(body.length()));
        request.append("\r\n");
        request.append("\r\n");

        request.append(body);

        if (!m_sock->send(request.data(), request.size()))
        {
            return CL_TRANSPORT_ERROR;
        }

        const int res = readHeaders();
        if ( res < 0 )
            return (CLHttpStatus)res;

        QList<QByteArray> strings = m_responseLine.split(' ');
        if (strings.size() < 2)
        {
            close();
            return CL_TRANSPORT_ERROR;
        }

        int responseCode = strings[1].toInt();

        switch(responseCode)
        {
            case CL_HTTP_SUCCESS:
            case CL_HTTP_BAD_REQUEST:
            {
                break;
            }

            case CL_HTTP_AUTH_REQUIRED:
            {
                getAuthInfo();
                if (recursive)
                {
                    close();
                    return doPOST(requestStr, body, false);
                }
                else
                    return CL_HTTP_AUTH_REQUIRED;
            }

            default:
            {
                close();
                return CL_TRANSPORT_ERROR;
            }
        }

        m_connected = true;

        // Http statuses: OK or BAD_REQUEST
        return (CLHttpStatus) responseCode;

    }
    catch (...)
    {
        close();
        return CL_TRANSPORT_ERROR;
    }
}

int CLSimpleHTTPClient::readHeaders()
{
    m_contentLen = 0;
    m_readed = 0;
    m_dataRestLen = 0;
    char* curPtr = m_headerBuffer;
    int left = sizeof(m_headerBuffer)-1;
    m_header.clear();
    m_responseLine.clear();
    int eofLen = 4;

    char* eofPos = 0;
    while (eofPos == 0)
    {
        int readed = m_sock->recv(curPtr, left);
        if (readed < 1)
            return CL_TRANSPORT_ERROR;
        curPtr += readed;
        left -= readed;
        *curPtr = 0; // end of text
        eofPos = strstr(m_headerBuffer, "\r\n\r\n");
        if (eofPos == 0) {
            eofPos = strstr(m_headerBuffer, "\n\n");
            if (eofPos)
                eofLen = 2;
        }
    }
    QList<QByteArray> lines = QByteArray(m_headerBuffer, eofPos-m_headerBuffer).split('\n');
    m_responseLine = lines[0];
    for (int i = 1; i < lines.size(); ++i)
    {
        QByteArray& line = lines[i];
        int delimPos = line.indexOf(':');
        if (delimPos == -1)
            m_header.insert(line.trimmed(), QByteArray());
        else
        {
            QByteArray paramName = line.left(delimPos).trimmed();
            QByteArray paramValue = line.mid(delimPos+1).trimmed();
            m_header.insert(paramName, paramValue);
            if (paramName == "Content-Length")
                m_contentLen = paramValue.toInt();
        }
    }
    if (eofPos+eofLen < curPtr) {
        m_dataRestPtr = eofPos+eofLen;
        m_dataRestLen = curPtr - m_dataRestPtr;
    }

    return CL_HTTP_SUCCESS;
}

QByteArray CLSimpleHTTPClient::getHeaderValue(const QByteArray& key)
{
    return m_header.value(key);
}

const QString& CLSimpleHTTPClient::localAddress() const
{
    return m_localAddress;
}

unsigned short CLSimpleHTTPClient::localPort() const
{
    return m_localPort;
}

CLHttpStatus CLSimpleHTTPClient::doGET(const QString& requestStr, bool recursive)
{
    return doGET(requestStr.toUtf8(), recursive);
}

CLHttpStatus CLSimpleHTTPClient::doGET(const QByteArray& _requestStr, bool recursive)
{
    QByteArray requestStr = _requestStr;
    if( !requestStr.startsWith('/') )
        requestStr.insert(0, '/');


    if (!m_sock)
        return CL_TRANSPORT_ERROR;

    try
    {
        if (!m_connected)
        {
            if (m_host.isEmpty() || !m_sock->connect(m_host, m_port))
            {
                return CL_TRANSPORT_ERROR;
            }
        }

        const SocketAddress& localHostAndPort = m_sock->getLocalAddress();
        m_localAddress = localHostAndPort.address.toString();
        m_localPort = localHostAndPort.port;

        QByteArray request;

        request.append("GET ");

        QByteArray encodedRequest = QUrl(QLatin1String(requestStr)).toString(QUrl::EncodeSpaces | QUrl::EncodeUnicode | QUrl::EncodeDelimiters).toLatin1();
        request.append(encodedRequest);

        request.append(" HTTP/1.1\r\n");
        request.append("Host: ");
        request.append(m_host.toUtf8());
        request.append("\r\n");

        addExtraHeaders(request);

        if (m_auth.user().length()>0 && mNonce.isEmpty())
        {
            request.append(basicAuth());
            request.append("\r\n");
        }
        else if (m_auth.password().length()>0 && !mNonce.isEmpty())
        {
            request.append(digestAccess(lit("GET"), QLatin1String(requestStr)));
        }

        request.append("\r\n");

        if (!m_sock->send(request.data(), request.size()))
        {
            return CL_TRANSPORT_ERROR;
        }

        const int res = readHeaders();
        if ( res < 0 )
            return (CLHttpStatus)res;

        //m_responseLine = m_responseLine.toLower();

        //got following status line: http/1.1 401 n/a, and this method returned CL_TRANSPORT_ERROR
        nx_http::StatusLine statusLine;
        if( !statusLine.parse( m_responseLine ) )
            return CL_TRANSPORT_ERROR;

        //if (!m_responseLine.contains("200 ok") && !m_responseLine.contains("204 no content"))// not ok
        if( statusLine.statusCode != nx_http::StatusCode::ok && statusLine.statusCode != nx_http::StatusCode::noContent )
        {
            close();

            //if (m_responseLine.contains("401 unauthorized"))
            if( statusLine.statusCode == nx_http::StatusCode::unauthorized )
            {
                getAuthInfo();

                if (recursive)
                {
                    close();
                    return doGET(requestStr, false);
                }
                else
                    return CL_HTTP_AUTH_REQUIRED;
            }
            //else if (m_responseLine.contains("not found"))
            else if( statusLine.statusCode == nx_http::StatusCode::notFound )
            {
                return CL_HTTP_NOT_FOUND;
            }
            else if( statusLine.statusCode == nx_http::StatusCode::notAllowed )
            {
                return CL_HTTP_NOT_ALLOWED;
            }
            else if( statusLine.statusCode == nx_http::StatusCode::forbidden )
            {
                return CL_HTTP_FORBIDDEN;
            }
            else if( statusLine.statusCode == nx_http::StatusCode::found )
            {
                return CL_HTTP_REDIRECT;
            }
            else if( statusLine.statusCode == nx_http::StatusCode::serviceUnavailable)
            {
                return CL_HTTP_SERVICEUNAVAILABLE;
            }

            return CL_TRANSPORT_ERROR;
        }

        m_connected = true;

        return CL_HTTP_SUCCESS;

    }
    catch (...)
    {
        close();
        return CL_TRANSPORT_ERROR;
    }
}

void CLSimpleHTTPClient::close()
{
    initSocket();
    m_connected = false;
}

void CLSimpleHTTPClient::readAll(QByteArray& data)
{
    if (m_dataRestLen)
    {
        data.append(m_dataRestPtr, m_dataRestLen);
        m_dataRestLen = 0;
    }

    if (m_contentLen && static_cast<int>(m_contentLen) == data.size())
        return;

    static const unsigned long BUFSIZE = 1024;
    int nRead;
    char buf[BUFSIZE];
    while ((nRead = read(buf, BUFSIZE)) > 0)
    {
        data.append(buf, nRead);
        if (m_contentLen && static_cast<int>(m_contentLen) == data.size())
            return;
    }
}

int CLSimpleHTTPClient::read(char* data, int max_len)
{
    if (!m_connected) return -1;

    int bufferRestLen = 0;
    if (m_dataRestLen)
    {
        bufferRestLen = qMin(m_dataRestLen, max_len);
        memcpy(data, m_dataRestPtr, bufferRestLen);
        data += bufferRestLen;
        m_dataRestPtr += bufferRestLen;
        m_dataRestLen -= bufferRestLen;
        max_len -= bufferRestLen;
        m_readed += bufferRestLen;
    }
    if (max_len == 0) {
        if (m_readed == m_contentLen)
            m_connected = false;
        return bufferRestLen;
    }

    int readed = m_sock->recv(data, max_len);
    if (readed<=0)
        close();

    if (readed > 0)
        m_readed+=readed;

    if (m_readed == m_contentLen)
        m_connected = false;

    if (bufferRestLen > 0)
        return bufferRestLen + qMax(readed, 0);
    else
        return readed;
}

// ===================================================================================
// ===================================================================================
void CLSimpleHTTPClient::getAuthInfo()
{
    QByteArray wwwAuth = m_header.value("WWW-Authenticate");
    if (!wwwAuth.toLower().startsWith("digest"))
        return;
    wwwAuth = wwwAuth.mid(QByteArray("digest").length());

    QList<QByteArray> authParams = wwwAuth.split(',');
    for (int i = 0; i < authParams.size(); ++i)
    {
        QList<QByteArray> param = nx::utils::smartSplit(authParams[i], '=');
        if (param.size() > 1)
        {
            param[0] = param[0].trimmed();
            param[1] = param[1].trimmed();
            param[1] = param[1].mid(1, param[1].length()-2);
            if (param[0] == QByteArray("realm"))
                mRealm = QLatin1String(param[1]);
            else if (param[0] == QByteArray("nonce"))
                mNonce = QLatin1String(param[1]);
            else if (param[0] == QByteArray("qop"))
                mQop = QLatin1String(param[1]);
        }
       // if (param.startsWith("digest")); /** Empty statement? */
    }
}

QByteArray CLSimpleHTTPClient::basicAuth(const QAuthenticator& auth)
{
    QByteArray lp;
    lp.append(auth.user().toUtf8());
    lp.append(':');
    lp.append(auth.password().toUtf8());

    QByteArray base64("Authorization: Basic ");
    base64.append(lp.toBase64());
    return base64;
}

QByteArray CLSimpleHTTPClient::basicAuth() const
{
    return basicAuth(m_auth);
}

QString CLSimpleHTTPClient::digestAccess(const QAuthenticator& auth, const QString& realm, const QString& nonce, const QString& method, const QString& url, bool isProxy)
{
    QString HA1= auth.user() + QLatin1Char(':') + realm + QLatin1Char(':') + auth.password();
    HA1 = QString::fromLatin1(QCryptographicHash::hash(HA1.toLatin1(), QCryptographicHash::Md5).toHex().constData());

    QString HA2 = method + QLatin1Char(':') + url;
    HA2 = QString::fromLatin1(QCryptographicHash::hash(HA2.toLatin1(), QCryptographicHash::Md5).toHex().constData());

    QString response = HA1 + QLatin1Char(':') + nonce + QLatin1Char(':') + HA2;

    response = QString::fromLatin1(QCryptographicHash::hash(response.toLatin1(), QCryptographicHash::Md5).toHex().constData());


    QString result;
    QTextStream str(&result);

    str << (isProxy ? "Proxy-Authorization" : "Authorization") <<
        ": Digest username=\"" << auth.user() << "\", realm=\"" << realm <<
        "\", nonce=\"" << nonce << "\", uri=\"" << url << "\", response=\"" << response << "\"\r\n";

    return result;

}

QString CLSimpleHTTPClient::digestAccess(const QString& method, const QString& url) const
{
    return digestAccess(m_auth, mRealm, mNonce, method, url);
}

void CLSimpleHTTPClient::addExtraHeaders(QByteArray& request)
{
    for (QMap<QByteArray,QByteArray>::iterator itr = m_additionHeaders.begin(); itr != m_additionHeaders.end(); ++itr)
    {
        request.append(itr.key());
        request.append(": ");
        request.append(itr.value());
        request.append("\r\n");
    }
}

QByteArray downloadFile(CLHttpStatus& status, const QString& fileName, const QString& host, int port, unsigned int timeout, const QAuthenticator& auth, int capacity)
{
    CLSimpleHTTPClient http (host, port, timeout, auth);
    status = http.doGET(fileName);


    QByteArray file;
    file.reserve(capacity);

    while (http.isOpened())
    {
        int curr_size = file.size();

        file.resize(curr_size + 1450);

        int readed = http.read(file.data() + curr_size, 1450);

        if (readed < 1450)
            file.resize(curr_size + readed);

    }

    return file;
}

bool uploadFile(const QString& fileName, const QString&  content, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth)
{
    CLSimpleHTTPClient http (host, port, timeout, auth);
    return http.doPOST(fileName, content) == CL_HTTP_SUCCESS;
}
