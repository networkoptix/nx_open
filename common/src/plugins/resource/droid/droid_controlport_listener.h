#include "utils/network/tcp_listener.h"
#include "utils/network/tcp_connection_processor.h"

#ifdef ENABLE_DROID

static const int DROID_CONTROL_TCP_SERVER_PORT = 5690;

class QnDroidControlPortListener: public QnTcpListener
{
public:
    QnDroidControlPortListener(const QHostAddress& address, int port);
    virtual ~QnDroidControlPortListener();
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(QSharedPointer<AbstractStreamSocket> clientSocket, QnTcpListener* owner) override;
};

class QnDroidControlPortProcessorPrivate;

class QnDroidControlPortProcessor: public QnTCPConnectionProcessor
{
public:
    QnDroidControlPortProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnDroidControlPortProcessor();
protected:
    virtual void run() override;

    Q_DECLARE_PRIVATE(QnDroidControlPortProcessor);
};

#endif // ENABLE_DROID
