#ifndef __EC2_TRANSACTION_SERVER_H__
#define __EC2_TRANSACTION_SERVER_H__

#include "nx/streaming/abstract_data_consumer.h"
#include "network/tcp_connection_processor.h"
#include "network/tcp_listener.h"

namespace ec2
{

class QnTransactionTcpProcessorPrivate;
class QnTransactionMessageBus;

class QnTransactionTcpProcessor: public QnTCPConnectionProcessor
{
public:
    QnTransactionTcpProcessor(
        QnTransactionMessageBus* messageBus,
        QSharedPointer<AbstractStreamSocket> socket,
        QnTcpListener* owner);
    virtual ~QnTransactionTcpProcessor();
    virtual bool isTakeSockOwnership() const override { return true; }
protected:
    virtual void run() override;
private:
    Q_DECLARE_PRIVATE(QnTransactionTcpProcessor);
};

}

#endif // __EC2_TRANSACTION_SERVER_H__
