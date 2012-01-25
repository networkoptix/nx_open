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
    m_auth(auth)
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

CLHttpStatus CLSimpleHTTPClient::getNextLine()
{
    m_line.clear();
    char prev = 0 , curr = 0;

    int readed = 0;

    while(1)
    {
        if (m_sock->recv(&curr,1)<0)
            return CL_TRANSPORT_ERROR;

        if (prev=='\r' && curr == '\n')
            break;

        if (prev==0 && curr == 0 && readed>1)
            break;

        //response << curr;
        m_line.append(curr);
        prev = curr;
        ++readed;

        if (readed > MAX_LINE_LENGTH)
            break;
    }

    return CL_HTTP_SUCCESS;

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

        if (CL_TRANSPORT_ERROR==getNextLine())
        {
            return CL_TRANSPORT_ERROR;
        }

        QList<QByteArray> strings = m_line.split(' ');
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

        if (readHeaders() == CL_TRANSPORT_ERROR)
            return CL_TRANSPORT_ERROR;

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
    m_contentType.clear();
    m_contentLen = 0;

    int pos;

    while(1)
    {

        pos = -1;

        if (CL_TRANSPORT_ERROR==getNextLine())
            return CL_TRANSPORT_ERROR;

        if (m_line.length()<3)// last line before data
            break;

        //Content-Type
        //ContentLength

        pos  = m_line.indexOf(':');

        if (pos>=0)
        {
            QByteArray name = m_line.left(pos).trimmed();
            QByteArray val = m_line.mid(pos+1, m_line.length()- (pos + 1 ) ).trimmed();
            m_header.insert(name, val );
            if (name == "Content-Length")
            {
                m_contentLen = val.toInt();
                m_readed = 0;
            }
        }

    }

    return 0;
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
            qDebug() << "OpenStream1";

            return CL_TRANSPORT_ERROR;
        }

        if (CL_TRANSPORT_ERROR==getNextLine())
        {
            return CL_TRANSPORT_ERROR;
        }


        if (!m_line.contains("200 OK"))// not ok
        {
            close();

            if (m_line.contains("401 Unauthorized"))
            {
                getAuthInfo();

                if (recursive)
                    return doGET(requestStr, false);
                else
                    return CL_HTTP_AUTH_REQUIRED;
            }

            return CL_TRANSPORT_ERROR;
        }

        if (readHeaders() == CL_TRANSPORT_ERROR)
            return CL_TRANSPORT_ERROR;


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
    static const unsigned long BUFSIZE = 1024;

    int nRead;

    char buf[BUFSIZE];
    while ((nRead = read(buf, BUFSIZE)) > 0)
    {
        data.append(buf, nRead);
    }
}

long CLSimpleHTTPClient::read(char* data, unsigned long max_len)
{
    if (!m_connected) return -1;

    int readed = 0;
    readed = m_sock->recv(data, max_len);


    if (readed<=0)
        close();

    m_readed+=readed;

    if (m_readed == m_contentLen)
        m_connected = false;


    return readed;
}

//===================================================================================
//===================================================================================
void CLSimpleHTTPClient::getAuthInfo()
{
    int pos;
    while(1)
    {

        pos = -1;

        if (CL_TRANSPORT_ERROR==getNextLine())
            return;

        if (m_line.length()<3)// last line before data
            break;

        if (m_line.contains("realm"))
        {
            mRealm = getParamFromString(m_line, "realm");
        }

        if (m_line.contains("nonce"))
        {
            mNonce = getParamFromString(m_line, "nonce");
        }

        if (m_line.contains("qop"))
        {
            mQop = getParamFromString(m_line, "qop");
        }

    }

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


QByteArray downloadFile(const QString& fileName, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth, int capacity)
{
    CLSimpleHTTPClient http (host, port, timeout, auth);
    http.doGET(fileName);

    

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