#include ".\simple_http_client.h"

#include <sstream>

using namespace std;

CLSimpleHTTPClient::CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth):
m_host(host),
m_port(port),
m_timeout(timeout),
m_auth(auth),
m_connected(false)
{
	m_sock.setTimeOut(timeout);
}

CLSimpleHTTPClient::~CLSimpleHTTPClient()
{

}


CLHttpStatus CLSimpleHTTPClient::getNextLine()
{
	m_line = "";
	QTextStream response(&m_line);
	char prev = 0 , curr = 0;

	while(1) 
	{
		if (m_sock.recv(&curr,1)<0)
			return CL_HTTP_HOST_NOT_AVAILABLE;

		

		if (prev=='\r' && curr == '\n')
			break;

		response << curr;
		prev = curr;
	}

	return CL_HTTP_SUCCESS;

}

CLHttpStatus CLSimpleHTTPClient::openStream()
{
	try
	{
		m_sock.connect(m_host.toString().toLatin1().data(), m_port);
		
		QString request;
		QTextStream os(&request);

		os << "GET /" << m_request<<" HTTP/1.1\r\n"
			<< "Host: "<< m_host.toString() << "\r\n";

		if (m_auth.password().length()>0)
		{
			QString lp = m_auth.user() + ":";
			lp += m_auth.password();

			QString base64 = "Authorization: Basic ";
			base64 += lp.toLatin1().toBase64().data();

			os << base64 << "\r\n";
		}


		os<< "\r\n";

		
		m_sock.send(request.toLatin1().data(), request.toLatin1().size());



		if (CL_HTTP_HOST_NOT_AVAILABLE==getNextLine())
			return CL_HTTP_HOST_NOT_AVAILABLE;


		if (!m_line.contains("200 OK"))// not ok
		{
			m_connected = false;

			if (m_line.contains("401 Unauthorized"))
				return CL_HTTP_AUTH_REQUIRED;



			return CL_HTTP_HOST_NOT_AVAILABLE;
		}


		m_contentType = "";
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
			
			pos  = m_line.indexOf(":");

			if (pos>=0) 
			{
				QString name = m_line.left(pos).trimmed();
				QString val = m_line.mid(pos+1, m_line.length()- (pos + 1 ) );
				CLAssociativeArray::put(name, val );
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


	return readed;
}

