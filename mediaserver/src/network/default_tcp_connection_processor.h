#ifndef __DEFAULT_TCP_CONNECTION_PROCESSOR
#define __DEFAULT_TCP_CONNECTION_PROCESSOR

#include "utils/network/tcp_connection_processor.h"

class QnDefaultTcpConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnDefaultTcpConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
protected:
    virtual void run() override;
};

#endif
