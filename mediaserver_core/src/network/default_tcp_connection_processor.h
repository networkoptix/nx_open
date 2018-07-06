#ifndef __DEFAULT_TCP_CONNECTION_PROCESSOR
#define __DEFAULT_TCP_CONNECTION_PROCESSOR

#include "network/tcp_connection_processor.h"

class QnDefaultTcpConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnDefaultTcpConnectionProcessor(QSharedPointer<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
protected:
    virtual void run() override;
};

#endif
