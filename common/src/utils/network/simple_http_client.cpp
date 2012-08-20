#include "simple_http_client.h"

#include <QtCore/QCryptographicHash>

#include <sstream>
#include "../common/util.h"


static const int MAX_LINE_LENGTH = 1024*16;

using namespace std;

CLSimpleHTTPClient::CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_connected(false),
    m_timeout(timeout),
    m_auth(auth),
    m_dataRestPtr(0),
    m_dataRestLen(0)
{
    initSocket();
}

void CLSimpleHTTPClient::initSocket()
{
    m_sock = TCPSocketPtr(new TCPSocket());

    m_sock->setReadTimeOut(m_timeout);
    m_sock->setWriteTimeOut(m_timeout);
}

CLSimpleHTTPClient::~CLSimpleHTTPClient()
{
}

QHostAddress CLSimpleHTTPClient::getLocalHost() const
{
    return QHostAddress(m_sock->getLocalAddress());
}

CLHttpStatus CLSimpleHTTPClient::doPOST(const QString& requestStr, const QString& body)
{
    return doPOST(requestStr.toUtf8(), body);
}

CLHttpStatus CLSimpleHTTPClient::doPOST(const QByteArray& requestStr, const QString& body)
{
    try
    {
        if (!m_connected)
        {
            if (!m_sock->connect(m_host.toString(), m_port))
            {
                return CL_TRANSPORT_ERROR;
            }
        }

        QByteArray request;
        request.append("POST /");
        request.append(requestStr);
        request.append(" HTTP/1.1\r\n");
        request.append("Host: ");
        request.append(m_host.toString().toUtf8());
        request.append("\r\n");
        request.append("User-Agent: Simple HTTP Client 1.0\r\n");
        request.append("Accept: */*\r\n");
        request.append("Content-Type: application/x-www-form-urlencoded\r\n");

        if (m_auth.user().length()>0 && mNonce.isEmpty())
        {
            request.append(basicAuth());
            request.append("\r\n");
        }
        else if (m_auth.password().length()>0 && !mNonce.isEmpty())
        {
            request.append(digestAccess(QLatin1String(request)));
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

        if (CL_TRANSPORT_ERROR==readHeaders())
            return CL_TRANSPORT_ERROR;

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

CLHttpStatus CLSimpleHTTPClient::doGET(const QString& requestStr, bool recursive)
{
    return doGET(requestStr.toUtf8(), recursive);
}

CLHttpStatus CLSimpleHTTPClient::doGET(const QByteArray& requestStr, bool recursive)
{
    try
    {
        if (!m_connected)
        {
            if (!m_sock->connect(m_host.toString(), m_port))
            {
                return CL_TRANSPORT_ERROR;
            }
        }

        QByteArray request;

        request.append("GET /");
        request.append(requestStr);
        request.append(" HTTP/1.1\r\n");
        request.append("Host: ");
        request.append(m_host.toString().toUtf8());
        request.append("\r\n");

        if (m_auth.user().length()>0 && mNonce.isEmpty())
        {
            request.append(basicAuth());
            request.append("\r\n");
        }
        else if (m_auth.password().length()>0 && !mNonce.isEmpty())
        {
            request.append(digestAccess(QLatin1String(requestStr)));
        }

        request.append("\r\n");

        if (!m_sock->send(request.data(), request.size()))
        {
            return CL_TRANSPORT_ERROR;
        }

        if (CL_TRANSPORT_ERROR==readHeaders())
            return CL_TRANSPORT_ERROR;

        m_responseLine = m_responseLine.toLower();

        if (!m_responseLine.contains("200 ok"))// not ok
        {
            close();

            if (m_responseLine.contains("401 unauthorized"))
            {
                getAuthInfo();

                if (recursive)
                    return doGET(requestStr, false);
                else
                    return CL_HTTP_AUTH_REQUIRED;
            }
            else if (m_responseLine.contains("not found"))
            {
                return CL_HTTP_NOT_FOUND;
            }
            else if (m_responseLine.contains("302 moved"))
            {
                return CL_HTTP_REDIRECT;
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

    static const unsigned long BUFSIZE = 1024;
    int nRead;
    char buf[BUFSIZE];
    while ((nRead = read(buf, BUFSIZE)) > 0)
    {
        data.append(buf, nRead);
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

//===================================================================================
//===================================================================================
void CLSimpleHTTPClient::getAuthInfo()
{
    QByteArray wwwAuth = m_header.value("WWW-Authenticate");
    if (!wwwAuth.toLower().startsWith("digest"))
        return;
    wwwAuth = wwwAuth.mid(QByteArray("digest").length());

    QList<QByteArray> authParams = wwwAuth.split(',');
    for (int i = 0; i < authParams.size(); ++i)
    {
        QList<QByteArray> param = authParams[i].split('=');
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

QString CLSimpleHTTPClient::digestAccess(const QAuthenticator& auth, const QString& realm, const QString& nonce, const QString& method, const QString& url)
{
    QString HA1= auth.user() + QLatin1Char(':') + realm + QLatin1Char(':') + auth.password();
    HA1 = QString::fromAscii(QCryptographicHash::hash(HA1.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString HA2 = method + QLatin1Char(':') + url;
    HA2 = QString::fromAscii(QCryptographicHash::hash(HA2.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString response = HA1 + QLatin1Char(':') + nonce + QLatin1Char(':') + HA2;

    response = QString::fromAscii(QCryptographicHash::hash(response.toAscii(), QCryptographicHash::Md5).toHex().constData());


    QString result;
    QTextStream str(&result);

    str << "Authorization: Digest username=\"" << auth.user() << "\",realm=\"" << realm << "\",nonce=\"" << nonce << "\",uri=\"" << url << "\",response=\"" << response << "\"\r\n";

    return result;

}

QString CLSimpleHTTPClient::digestAccess(const QString& request) const
{
    return digestAccess(m_auth, mRealm, mNonce, QLatin1String("GET"), QLatin1Char('/') + request);
}


QByteArray downloadFile(CLHttpStatus& status, const QString& fileName, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth, int capacity)
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