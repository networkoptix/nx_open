/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"

#include <api/app_server_connection.h>

#include "transaction/transaction_message_bus.h"
#include "common/common_module.h"
#include "managers/time_manager.h"
#include "mutex/distributed_mutex.h"
#include "mutex/distributed_mutex_manager.h"

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
        ec2::QnDistributedMutexManager::initStaticInstance(new ec2::QnDistributedMutexManager());

        QnTransactionMessageBus::instance()->setHandler( notificationManager() );
    }

    RemoteEC2Connection::~RemoteEC2Connection()
    {
        QnTransactionMessageBus::instance()->removeConnectionFromPeer( m_peerUrl );
        QnTransactionMessageBus::instance()->removeHandler( notificationManager() );

        //TODO #ak next call can be placed here just because we always have just one connection to EC
        TimeSynchronizationManager::instance()->forgetSynchronizedTime();
    }

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void RemoteEC2Connection::startReceivingNotifications() {

        // in remote mode we are always working as a client
        ApiPeerData localPeer(qnCommon->moduleGUID(), Qn::PT_DesktopClient);

        QUuid videowallGuid = QnAppServerConnectionFactory::videowallGuid();
        if (!videowallGuid.isNull())
            localPeer.peerType = Qn::PT_VideowallClient;
        
        QnTransactionMessageBus::instance()->setLocalPeer(localPeer);
        QnTransactionMessageBus::instance()->start();

        QUrl url(m_queryProcessor->getUrl());
        url.setScheme( m_connectionInfo.allowSslConnections ? lit("https") : lit("http") );
        url.setPath("ec2/events");
        QUrlQuery q;
        q.addQueryItem("guid", qnCommon->moduleGUID().toString());
        url.setQuery(q);
        m_peerUrl = url;
        QnTransactionMessageBus::instance()->addConnectionToPeer(url);
    }
}
