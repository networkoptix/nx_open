#ifndef __REST_SERVER_H__
#define __REST_SERVER_H__

#include "utils/network/tcp_listener.h"

static const int DEFAULT_REST_PORT = 8080;

typedef QList<QPair<QString, QString> > QnParamList;

class QnRestRequestHandler
{
public:
    virtual int executeGet(const QnParamList& params, QByteArray& result) = 0;
    virtual int executePost(const QnParamList& params, const QByteArray& body, QByteArray& result) = 0;
};

class QnRestServer : public QnTcpListener
{
public:
    typedef QMap<QString, QnRestRequestHandler*> Handlers;


    explicit QnRestServer(const QHostAddress& address, int port = DEFAULT_REST_PORT);
    virtual ~QnRestServer();

    void registerHandler(const QString& path, QnRestRequestHandler* handler);
    QnRestRequestHandler* findHandler(const QString& path);
protected:
    virtual CLLongRunnable* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
private:
     Handlers m_handlers;
};

#endif // __REST_SERVER_H__
