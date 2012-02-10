#ifndef __REST_SERVER_H__
#define __REST_SERVER_H__

#include <QList>
#include <QPair>
#include <QString>
#include <QByteArray>
#include "request_handler.h"
#include "utils/network/tcp_listener.h"

static const int DEFAULT_REST_PORT = 8080;

class QnRestServer : public QnTcpListener
{
public:
    typedef QMap<QString, QnRestRequestHandler*> Handlers;


    explicit QnRestServer(const QHostAddress& address, int port = DEFAULT_REST_PORT);
    virtual ~QnRestServer();

    void registerHandler(const QString& path, QnRestRequestHandler* handler);
    QnRestRequestHandler* findHandler(QString path);
    const Handlers& allHandlers() const;
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
private:
     Handlers m_handlers;
};

#endif // __REST_SERVER_H__
