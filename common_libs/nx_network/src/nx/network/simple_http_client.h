#ifndef __SIMPLE_HTTP_CLIENT__
#define __SIMPLE_HTTP_CLIENT__

#include <QtNetwork/QAuthenticator>
#include <QtNetwork/QHostAddress>
#include <QtCore/QHash>
#include <QtCore/QString>
#include <QSharedPointer>

#include <nx/network/socket_common.h>

#include "socket.h"

// TODO: #Elric this class is so bad interface-wise that I don't even want to fix it.

enum CLHttpStatus
{
    CL_HTTP_SUCCESS = 200,
    CL_HTTP_REDIRECT = 302,
    CL_HTTP_BAD_REQUEST = 400,
    CL_HTTP_AUTH_REQUIRED = 401,
    CL_HTTP_FORBIDDEN = 403,
    CL_HTTP_NOT_FOUND = 404,
    CL_HTTP_NOT_ALLOWED = 405,
    CL_HTTP_SERVICEUNAVAILABLE = 503,

    CL_TRANSPORT_ERROR = -1
};

QString NX_NETWORK_API toString( CLHttpStatus status );

typedef QSharedPointer<AbstractStreamSocket> TCPSocketPtr;

/**
 * This class is deprecated. Please, use nx_http::HttpClient or nx_http::AsyncHttpClient.
 */
class NX_NETWORK_API CLSimpleHTTPClient
{
    // TODO: #Elric #enum
    enum { Basic, Digestaccess };

public:
    /*!
        \param timeout Timeout in milliseconds to be used as socket's read and write timeout
    */
    CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);
    CLSimpleHTTPClient(const QUrl& url, unsigned int timeout, const QAuthenticator& auth);

    /*!
        \param timeout Timeout in milliseconds to be used as socket's read and write timeout
    */
    CLSimpleHTTPClient(const QString& host, int port, unsigned int timeout, const QAuthenticator& auth);
    ~CLSimpleHTTPClient();

    CLHttpStatus doGET(const QString& request, bool recursive = true);
    CLHttpStatus doPOST(const QString& request, const QString& body, bool recursive = true);

    CLHttpStatus doGET(const QByteArray& request, bool recursive = true);
    CLHttpStatus doPOST(const QByteArray& request, const QString& body, bool recursive = true);

    void addHeader(const QByteArray& key, const QByteArray& value);

    QHostAddress getLocalHost() const;

    bool isOpened()const{return m_connected;}

    void readAll(QByteArray& data);

    int read(char* data, int max_len);

    void close();

    TCPSocketPtr getSocket() { return m_sock; }

    QHash<QByteArray, QByteArray> header() const
    {
        return m_header;
    }

    unsigned int getContentLen() const
    {
        return m_contentLen;
    }

    static QByteArray basicAuth(const QAuthenticator& auth);
    /*!
        \param isProxy If \a true, this method adds header Proxy-Authorization, otherwise Authorization
    */
    static QString digestAccess(const QAuthenticator& auth, const QString& realm, const QString& nonce, const QString& method, const QString& url, bool isProxy = false);
    QByteArray getHeaderValue(const QByteArray& key);

    const QString& localAddress() const;
    unsigned short localPort() const;

    QString mRealm;
    QString mNonce; // TODO: #Elric wtf? total incapsulation failure.
    QString mQop;

    QString host() const { return m_host; }
    int port() const { return m_port; }
    int timeout() const { return m_timeout; }
    QAuthenticator auth() const { return m_auth; }

private:
    void initSocket( bool ssl = false );

    void getAuthInfo();

    QByteArray basicAuth() const;
    QString digestAccess(const QString& method, const QString& url) const;

    int readHeaders();
    void addExtraHeaders(QByteArray& request);

private:
    const QString m_host;
    int m_port;

    QHash<QByteArray, QByteArray> m_header;
    unsigned int m_contentLen;
    unsigned int m_readed;

    TCPSocketPtr m_sock;
    bool m_connected;

    unsigned int m_timeout;
    QAuthenticator m_auth;
    char m_headerBuffer[1024*16];
    QByteArray m_responseLine;
    char* m_dataRestPtr;
    int m_dataRestLen;
    QMap<QByteArray,QByteArray> m_additionHeaders;
    QString m_localAddress;
    unsigned short m_localPort;
};

/*!
    \param timeout Timeout in milliseconds to be used as socket's read and write timeout
*/
QByteArray NX_NETWORK_API downloadFile(
	CLHttpStatus& status, const QString& fileName,
	const QString& host, int port,
	unsigned int timeout, const QAuthenticator& auth,
	int capacity = 2000);

/*!
    \param timeout Timeout in milliseconds to be used as socket's read and write timeout
*/
bool NX_NETWORK_API uploadFile(
	const QString& fileName, const QString&  content,
	const QHostAddress& host, int port,
	unsigned int timeout, const QAuthenticator& auth);



#endif //__SIMPLE_HTTP_CLIENT__
