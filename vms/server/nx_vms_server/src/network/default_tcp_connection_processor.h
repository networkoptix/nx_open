#pragma once

#include "network/tcp_connection_processor.h"

class QnDefaultTcpConnectionProcessor: virtual public QnTCPConnectionProcessor
{
public:
    QnDefaultTcpConnectionProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> socket, QnTcpListener* owner);
protected:
    virtual void run() override;
};
