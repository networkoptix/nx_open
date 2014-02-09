#ifndef __EC2_TRANSACTION_SERVER_H__
#define __EC2_TRANSACTION_SERVER_H__

#include "utils/network/tcp_connection_processor.h"
#include "core/dataconsumer/abstract_data_consumer.h"
#include "utils/network/tcp_listener.h"

namespace ec2
{

class QnTransactionTcpProcessorPrivate;

class QnTransactionTcpProcessor: public QnTCPConnectionProcessor
{
public:
    QnTransactionTcpProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    virtual ~QnTransactionTcpProcessor();
    virtual bool isTakeSockOwnership() const override { return true; }
protected:
    virtual void run() override;
private:
    Q_DECLARE_PRIVATE(QnTransactionTcpProcessor);
};

}

#endif // __EC2_TRANSACTION_SERVER_H__
