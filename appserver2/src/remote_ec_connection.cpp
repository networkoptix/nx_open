/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"
#include "transaction/transaction_message_bus.h"


namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection(
        const FixedUrlClientQueryProcessorPtr& queryProcessor,
        const ResourceContext& resCtx,
        const QnConnectionInfo& connectionInfo )
    :
        BaseEc2Connection<FixedUrlClientQueryProcessor>( queryProcessor.get(), resCtx ),
        m_queryProcessor( queryProcessor ),
        m_connectionInfo( connectionInfo )
    {
        connect(m_transactionMsg, &QnTransactionMessageBus::gotTransaction, this, &RemoteEC2Connection::at_gotTransaction, Qt::DirectConnection);
    }

    void RemoteEC2Connection::at_gotTransaction(QByteArray tran)
    {

    }

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        return m_connectionInfo;
    }
}
