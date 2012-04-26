#include "utils/network/tcp_listener.h"

static const int DEFAUT_PROXY_PORT = 7009;

class QnProxyListener: public QnTcpListener
{
public:
    explicit QnProxyListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAUT_PROXY_PORT);
    virtual ~QnProxyListener();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
};
