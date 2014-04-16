#include "utils/network/tcp_listener.h"

static const int DEFAULT_PROXY_PORT = 7301;

class QnProxyListener: public QnTcpListener
{
public:
    explicit QnProxyListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAULT_PROXY_PORT);
    virtual ~QnProxyListener();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner);
};
