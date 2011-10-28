#include "utils/network/tcp_listener.h"

class QnRtspListener: public QnTcpListener
{
public:
    static const int DEFAULT_RTSP_PORT = 554;

    explicit QnRtspListener(const QHostAddress& address = QHostAddress::Any, int port = DEFAULT_RTSP_PORT);
    virtual ~QnRtspListener();
protected:
    virtual CLLongRunnable* createRequestProcessor(TCPSocket* clientSocket, QnTcpListener* owner);
};
