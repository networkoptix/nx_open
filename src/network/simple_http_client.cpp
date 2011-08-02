#include "simple_http_client.h"

#include <QtCore/QCryptographicHash>

#include <sstream>
#include "util.h"

using namespace std;

CLSimpleHTTPClient::CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
    m_host(host),
    m_port(port),
    m_connected(false),
    m_timeout(timeout),
    m_auth(auth)
{
    m_sock.setTimeOut(timeout);
}

CLSimpleHTTPClient::~CLSimpleHTTPClient()
{
}

CLHttpStatus CLSimpleHTTPClient::getNextLine()
{
	m_line.clear();
	QTextStream response(&m_line);
	char prev = 0 , curr = 0;

	int readed = 0;

	while(1)
	{
		if (m_sock.recv(&curr,1)<0)
			return CL_HTTP_HOST_NOT_AVAILABLE;

		if (prev=='\r' && curr == '\n')
			break;

		if (prev==0 && curr == 0 && readed>1)
			break;

		response << curr;
		prev = curr;
		++readed;
	}

	return CL_HTTP_SUCCESS;

}

CLHttpStatus CLSimpleHTTPClient::openStream(bool recursive)
{

	try
	{
		if (!m_sock.connect(m_host.toString().toLatin1().data(), m_port))
			return CL_HTTP_HOST_NOT_AVAILABLE;

		QString request;
		QTextStream os(&request);

		os << "GET /" << m_request<<" HTTP/1.1\r\n"
			<< "Host: "<< m_host.toString() << "\r\n";

		if (m_auth.password().length()>0 && mNonce.isEmpty())
		{
			os << basicAuth() << "\r\n";
		}
		else if (m_auth.password().length()>0 && !mNonce.isEmpty())
		{
			os << digestAccess();
		}

		os<< "\r\n";

        if (!m_sock.send(request.toLatin1().data(), request.toLatin1().size()))
            return CL_HTTP_HOST_NOT_AVAILABLE;

		if (CL_HTTP_HOST_NOT_AVAILABLE==getNextLine())
			return CL_HTTP_HOST_NOT_AVAILABLE;

		if (!m_line.contains(QLatin1String("200 OK")))// not ok
		{
			m_connected = false;

            if (m_line.contains(QLatin1String("401 Unauthorized")))
            {
                getAuthInfo();

                if (recursive)
                    return openStream(false);
                else
                    return CL_HTTP_AUTH_REQUIRED;
            }

			return CL_HTTP_HOST_NOT_AVAILABLE;
		}

		m_contentType.clear();
		m_contentLen = 0;

		int pos;

		while(1)
		{

			pos = -1;

			if (CL_HTTP_HOST_NOT_AVAILABLE==getNextLine())
				return CL_HTTP_HOST_NOT_AVAILABLE;

			if (m_line.length()<3)// last line before data
				break;

			//Content-Type
			//ContentLength

			pos  = m_line.indexOf(QLatin1Char(':'));

			if (pos>=0)
			{
				QString name = m_line.left(pos).trimmed();
				QString val = m_line.mid(pos+1, m_line.length()- (pos + 1 ) );
				CLAssociativeArray::put(name, val );
				if (name==QLatin1String("Content-Length"))
				{
					m_contentLen = val.toInt();
					m_readed = 0;
				}
			}

		}

		m_connected = true;
		return CL_HTTP_SUCCESS;

	}
	catch (...)
	{
		m_connected = false;
		return CL_HTTP_HOST_NOT_AVAILABLE;
	}

}

long CLSimpleHTTPClient::read(char* data, unsigned long max_len)
{
	if (!m_connected) return -1;

	int readed = 0;
	readed = m_sock.recv(data, max_len);


	if (readed<=0)
		m_connected = false;

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

        if (CL_HTTP_HOST_NOT_AVAILABLE==getNextLine())
            return;

        if (m_line.length()<3)// last line before data
            break;

        if (m_line.contains(QLatin1String("realm")))
        {
            mRealm = getParamFromString(m_line, QLatin1String("realm"));
        }

        if (m_line.contains(QLatin1String("nonce")))
        {
            mNonce = getParamFromString(m_line, QLatin1String("nonce"));
        }

        if (m_line.contains(QLatin1String("qop")))
        {
            mQop = getParamFromString(m_line, QLatin1String("qop"));
        }

    }

}

QString CLSimpleHTTPClient::basicAuth() const
{
    QString lp = m_auth.user() + QLatin1Char(':') + m_auth.password();

    QString base64 = QLatin1String("Authorization: Basic ");
    base64 += QString::fromAscii(lp.toLatin1().toBase64().constData());

    return base64;
}

QString CLSimpleHTTPClient::digestAccess() const
{
    QString HA1= m_auth.user() + QLatin1Char(':') + mRealm + QLatin1Char(':') + m_auth.password();
    HA1 = QString::fromAscii(QCryptographicHash::hash(HA1.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString HA2 = QLatin1String("GET:/") + m_request;
    HA2 = QString::fromAscii(QCryptographicHash::hash(HA2.toAscii(), QCryptographicHash::Md5).toHex().constData());

    QString response = HA1 + QLatin1Char(':') + mNonce + QLatin1Char(':') + HA2;

    response = QString::fromAscii(QCryptographicHash::hash(response.toAscii(), QCryptographicHash::Md5).toHex().constData());


    QString result;
    QTextStream str(&result);

    str << "Authorization: Digest username=\"" << m_auth.user() << "\",realm=\"" << mRealm << "\",nonce=\"" << mNonce << "\",uri=\"/" << m_request << "\",response=\"" << response << "\"\r\n";

    return result;
}
