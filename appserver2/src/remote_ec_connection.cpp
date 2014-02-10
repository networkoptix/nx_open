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
        QnTransactionMessageBus::instance()->setHandler(this);
        QUrl url(m_queryProcessor->getUrl());
        url.setPath("ec2/events");
    }

    RemoteEC2Connection::~RemoteEC2Connection()
    {
        QUrl url(m_queryProcessor->getUrl());
        url.setPath("ec2/events");
        QnTransactionMessageBus::instance()->removeConnectionFromPeer(url);
    }

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void RemoteEC2Connection::startReceivingNotifications()
    {
        QnTransactionMessageBus::instance()->addConnectionToPeer(m_queryProcessor->getUrl());
    }
}
