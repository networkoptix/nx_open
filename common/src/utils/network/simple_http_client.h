#ifndef __SIMPLE_HTTP_CLIENT__
#define __SIMPLE_HTTP_CLIENT__

#include <QtCore/QHash>

#include "socket.h"

enum CLHttpStatus
{
    CL_HTTP_SUCCESS = 200,
    CL_HTTP_BAD_REQUEST = 400,
    CL_HTTP_AUTH_REQUIRED = 401,
    CL_HTTP_NOT_FOUND,

    CL_TRANSPORT_ERROR = -1
};

class CLSimpleHTTPClient
{
    enum { Basic, Digestaccess };

public:
    CLSimpleHTTPClient(const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);
    ~CLSimpleHTTPClient();

    CLHttpStatus doGET(const QString& request, bool recursive = true);
    CLHttpStatus doPOST(const QString& request, const QString& body);

    CLHttpStatus doGET(const QByteArray& request, bool recursive = true);
    CLHttpStatus doPOST(const QByteArray& request, const QString& body);

    QHostAddress getLocalHost() const;

    bool isOpened()const{return m_connected;}

    void readAll(QByteArray& data);

    int read(char* data, int max_len);

    void close();

    QHash<QByteArray, QByteArray> header() const
    {
        return m_header;
    }

    unsigned int getContentLen() const
    {
        return m_contentLen;
    }

    static QByteArray basicAuth(const QAuthenticator& auth);


    QString mRealm;
    QString mNonce;
    QString mQop;


private:
    void initSocket();

    void getAuthInfo();

    QByteArray basicAuth() const;
    QString digestAccess(const QString&) const;

    int readHeaders();

private:

    QHostAddress m_host;
    int m_port;

    QHash<QByteArray, QByteArray> m_header;
    unsigned int m_contentLen;
    unsigned int m_readed;

    typedef QSharedPointer<TCPSocket> TCPSocketPtr;

    TCPSocketPtr m_sock;
    bool m_connected;

    unsigned int m_timeout;
    QAuthenticator m_auth;
    char m_headerBuffer[1024*16];
    QByteArray m_responseLine;
    char* m_dataRestPtr;
    int m_dataRestLen;
};

QByteArray downloadFile(CLHttpStatus& status, const QString& fileName, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth, int capacity = 2000);

bool uploadFile(const QString& fileName, const QString&  content, const QHostAddress& host, int port, unsigned int timeout, const QAuthenticator& auth);



#endif //__SIMPLE_HTTP_CLIENT__
