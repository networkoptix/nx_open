/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"

#include <api/app_server_connection.h>

#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "managers/time_manager.h"

namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection(
        const FixedUrlClientQueryProcessorPtr& queryProcessor,
        const ResourceContext& resCtx,
        const QnConnectionInfo& connectionInfo )
    :
        base_type( queryProcessor.get(), resCtx ),
        m_queryProcessor( queryProcessor ),
        m_connectionInfo( connectionInfo ),
        m_notificationReceiverID( 0 )
    {
        m_notificationReceiverID = QnTransactionMessageBus::instance()->addHandler( notificationManager() );
    }

    RemoteEC2Connection::~RemoteEC2Connection()
    {
        if( QnTransactionMessageBus::instance() && m_notificationReceiverID > 0 )
        {
            QnTransactionMessageBus::instance()->removeHandler( m_notificationReceiverID );
            m_notificationReceiverID = 0;
        }
    }

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    QString RemoteEC2Connection::authInfo() const
    {
        return m_connectionInfo.ecUrl.password();
    }

    void RemoteEC2Connection::startReceivingNotifications() {

        base_type::startReceivingNotifications();

        QUrl url(m_queryProcessor->getUrl());
        url.setScheme( m_connectionInfo.allowSslConnections ? lit("https") : lit("http") );
        url.setPath("ec2/events");
        QUrlQuery q;
        q.addQueryItem("guid", qnCommon->moduleGUID().toString());
        q.addQueryItem("runtime-guid", qnCommon->runningInstanceGUID().toString());
        url.setQuery(q);
        m_peerUrl = url;
        QnTransactionMessageBus::instance()->addConnectionToPeer(url);
    }

    void RemoteEC2Connection::stopReceivingNotifications() {
        base_type::stopReceivingNotifications();
        if (QnTransactionMessageBus::instance()) {
            QnTransactionMessageBus::instance()->removeConnectionFromPeer( m_peerUrl );
            if( m_notificationReceiverID > 0 )
            {
                QnTransactionMessageBus::instance()->removeHandler( m_notificationReceiverID );
                m_notificationReceiverID = 0;
            }
        }

        //TODO #ak next call can be placed here just because we always have just one connection to EC
        TimeSynchronizationManager::instance()->forgetSynchronizedTime();
    }

}
