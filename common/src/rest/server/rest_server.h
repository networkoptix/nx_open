#ifndef __REST_SERVER_H__
#define __REST_SERVER_H__

#include <QtCore/QList>
#include <QtCore/QPair>
#include <QtCore/QString>
#include <QtCore/QByteArray>
#include "request_handler.h"
#include "utils/network/tcp_listener.h"

static const int DEFAULT_REST_PORT = 8080;

class QnRestServer : public QnTcpListener
{
public:
    explicit QnRestServer(const QHostAddress& address, int port = DEFAULT_REST_PORT);
    virtual ~QnRestServer();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) override;
};

#endif // __REST_SERVER_H__
