#ifndef __SIMPLE_HTTP_CLIENT__
#define __SIMPLE_HTTP_CLIENT__

#include "socket.h"
#include "base/associativearray.h"

enum CLHttpStatus {CL_HTTP_SUCCESS, CL_HTTP_AUTH_REQUIRED, CL_HTTP_HOST_NOT_AVAILABLE};

class CLSimpleHTTPClient : public CLAssociativeArray
{
    enum {Basic , Digestaccess };
public:	
	CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);
	~CLSimpleHTTPClient();

	void setRequestLine(const QString& request){m_request = request;};

	CLHttpStatus openStream(bool recursive = true);
	bool isOpened()const{return m_connected;} ;

	long read(char* data, unsigned long max_len);

	QString getContentType() const 
	{
		return m_contentType;
	}

	unsigned int getContentLen() const
	{
		return m_contentLen;
	}

    QString mRealm;
    QString mNonce;
    QString mQop;


private:
	CLHttpStatus getNextLine();
    void getAuthInfo();

    QString basicAuth() const;
    QString digestAccess() const;

private:
    QString m_line;

	QHostAddress m_host;
	int m_port;

	QString m_request;
	QString m_contentType;
	unsigned int m_contentLen;
    unsigned int m_readed;

	TCPSocket m_sock;
	bool m_connected;

	unsigned int m_timeout;
	QAuthenticator m_auth;
};

#endif //__SIMPLE_HTTP_CLIENT__
