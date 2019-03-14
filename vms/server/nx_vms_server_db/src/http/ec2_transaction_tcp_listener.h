#pragma once

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"

namespace ec2
{

class QnTransactionTcpProcessorPrivate;
class ServerTransactionMessageBus;

class QnTransactionTcpProcessor: public QnTCPConnectionProcessor
{
public:
    QnTransactionTcpProcessor(
        ServerTransactionMessageBus* messageBus,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnTcpListener* owner);
    virtual ~QnTransactionTcpProcessor();

protected:
    virtual void run() override;

private:
    Q_DECLARE_PRIVATE(QnTransactionTcpProcessor);
};

}
