#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"

static const int DROID_CONTROL_TCP_SERVER_PORT = 5690;

class QnDroidControlPortListener: public QnTcpListener
{
public:
    QnDroidControlPortListener(const QHostAddress& address, int port);
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner) override;
};

class QnDroidControlPortProcessor: public QnTCPConnectionProcessor
{
public:
    QnDroidControlPortProcessor(TCPSocket* socket, QnTcpListener* owner);
protected:
    virtual void run() override;

    QN_DECLARE_PRIVATE_DERIVED(QnDroidControlPortProcessor);
};
