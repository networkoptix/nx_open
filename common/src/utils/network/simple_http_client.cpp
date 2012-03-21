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
            if (!m_sock->connect(m_host.toString().toLatin1().data(), m_port))
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
            request.append(digestAccess(request));
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
        int delimPos = line.indexOf('=');
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
            if (!m_sock->connect(m_host.toString().toLatin1().data(), m_port))
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
            request.append(digestAccess(request));
        }

        request.append("\r\n");

        if (!m_sock->send(request.data(), request.size()))
        {
            return CL_TRANSPORT_ERROR;
        }

        if (CL_TRANSPORT_ERROR==readHeaders())
            return CL_TRANSPORT_ERROR;


        if (!m_responseLine.contains("200 OK"))// not ok
        {
            close();

            if (m_responseLine.contains("401 Unauthorized"))
            {
                getAuthInfo();

                if (recursive)
                    return doGET(requestStr, false);
                else
                    return CL_HTTP_AUTH_REQUIRED;
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
    mRealm = m_header.value("realm");
    mNonce = m_header.value("nonce");
    mQop = m_header.value("qop");
}

QByteArray CLSimpleHTTPClient::basicAuth() const
{
    QByteArray lp;
    lp.append(m_auth.user().toUtf8());
    lp.append(':');
    lp.append(m_auth.password().toUtf8());

    QByteArray base64("Authorization: Basic ");
    base64.append(lp.toBase64());
    return base64;
}

QString CLSimpleHTTPClient::digestAccess(const QString& request) const
{
    QString HA1= m_auth.user() + QLatin1Char(':') + mRealm + QLatin1Char(':') + m_auth.password();
    HA1 = QString::fromAscii(QCryptographicHash::hash(HA1.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString HA2 = QLatin1String("GET:/") + request;
    HA2 = QString::fromAscii(QCryptographicHash::hash(HA2.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString response = HA1 + QLatin1Char(':') + mNonce + QLatin1Char(':') + HA2;

    response = QString::fromAscii(QCryptographicHash::hash(response.toAscii(), QCryptographicHash::Md5).toHex().constData());


    QString result;
    QTextStream str(&result);

    str << "Authorization: Digest username=\"" << m_auth.user() << "\",realm=\"" << mRealm << "\",nonce=\"" << mNonce << "\",uri=\"/" << request << "\",response=\"" << response << "\"\r\n";

    return result;
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