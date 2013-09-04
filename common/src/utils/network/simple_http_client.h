#ifndef __SIMPLE_HTTP_CLIENT__
#define __SIMPLE_HTTP_CLIENT__

#include <QAuthenticator>
#include <QtCore/QHash>
#include <QString>
#include <QSharedPointer>

#include "socket.h"

enum CLHttpStatus
{
    CL_HTTP_SUCCESS = 200,
    CL_HTTP_REDIRECT = 302,
    CL_HTTP_BAD_REQUEST = 400,
    CL_HTTP_AUTH_REQUIRED = 401,
    CL_HTTP_NOT_FOUND,

    CL_TRANSPORT_ERROR = -1
};

QString toString( CLHttpStatus status );

typedef typedef QSharedPointer<AbstractStreamSocket> TCPSocketPtr;

class CLSimpleHTTPClient
{
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
    CLHttpStatus doPOST(const QString& request, const QString& body);

    CLHttpStatus doGET(const QByteArray& request, bool recursive = true);
    CLHttpStatus doPOST(const QByteArray& request, const QString& body);

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
    static QString digestAccess(const QAuthenticator& auth, const QString& realm, const QString& nonce, const QString& method, const QString& url);
    QByteArray getHeaderValue(const QByteArray& key);

    const QString& localAddress() const;
    unsigned short localPort() const;

    QString mRealm;
    QString mNonce;
    QString mQop;


private:
    void initSocket();

    void getAuthInfo();

    QByteArray basicAuth() const;
    QString digestAccess(const QString&) const;

    int readHeaders();
    void addExtraHeaders(QByteArray& request);
private:

    QHostAddress m_host;
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
QByteArray downloadFile(CLHttpStatus& status, const QString& fileName, const QString& host, int port, unsigned int timeout, const QAuthenticator& auth, int capacity = 2000);

/*!
    \param timeout Timeout in milliseconds to be used as socket's read and write timeout
*/
bool uploadFile(const QString& fileName, const QString&  content, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);



#endif //__SIMPLE_HTTP_CLIENT__
