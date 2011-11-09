#ifndef __SIMPLE_HTTP_CLIENT__
#define __SIMPLE_HTTP_CLIENT__

#include "socket.h"
#include "../common/associativearray.h"


enum CLHttpStatus
{
    CL_HTTP_SUCCESS = 200,
    CL_HTTP_BAD_REQUEST = 400,
    CL_HTTP_AUTH_REQUIRED = 401,

    CL_TRANSPORT_ERROR = -1
};

class CLSimpleHTTPClient : public QnAssociativeArray
{
    enum {Basic , Digestaccess };
public:	
	CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);
	~CLSimpleHTTPClient();

    CLHttpStatus doGET(const QString& request, bool recursive = true);
    CLHttpStatus doPOST(const QString& request, const QString& body);

    CLHttpStatus doGET(const QByteArray& request, bool recursive = true);
    CLHttpStatus doPOST(const QByteArray& request, const QString& body);

    bool isOpened()const{return m_connected;}

    void readAll(QByteArray& data);

	long read(char* data, unsigned long max_len);

    void close();

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
    void initSocket();

    CLHttpStatus getNextLine();
    void getAuthInfo();

    QString basicAuth() const;
    QString digestAccess(const QString&) const;

    int readHeaders();

private:
    QString m_line;

	QHostAddress m_host;
	int m_port;

	QString m_contentType;
	unsigned int m_contentLen;
    unsigned int m_readed;

    typedef QSharedPointer<TCPSocket> TCPSocketPtr;

    TCPSocketPtr m_sock;
	bool m_connected;

	unsigned int m_timeout;
	QAuthenticator m_auth;
};

#endif //__SIMPLE_HTTP_CLIENT__
