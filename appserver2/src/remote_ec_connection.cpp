/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"

#include <api/app_server_connection.h>

#include <transaction/message_bus_adapter.h>
#include <nx/p2p/p2p_message_bus.h>
#include "common/common_module.h"
#include "managers/time_manager.h"

namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection(
        const AbstractECConnectionFactory* connectionFactory,
        const QnUuid& remotePeerId,
        const FixedUrlClientQueryProcessorPtr& queryProcessor,
        const QnConnectionInfo& connectionInfo )
    :
        base_type(connectionFactory, queryProcessor.get() ),
        m_queryProcessor( queryProcessor ),
        m_connectionInfo( connectionInfo ),
        m_remotePeerId(remotePeerId)
    {
    }

    RemoteEC2Connection::~RemoteEC2Connection()
    {}

    QnConnectionInfo RemoteEC2Connection::connectionInfo() const
    {
        return m_connectionInfo;
    }

    void RemoteEC2Connection::updateConnectionUrl(const nx::utils::Url &url)
    {
        m_connectionInfo.ecUrl = url;
    }

    void RemoteEC2Connection::startReceivingNotifications()
    {
        m_connectionFactory->messageBus()->init(
            m_connectionInfo.p2pMode ? MessageBusType::P2pMode : MessageBusType::LegacyMode);
        m_connectionFactory->messageBus()->setHandler(notificationManager());

        base_type::startReceivingNotifications();

        nx::utils::Url url(m_queryProcessor->getUrl());
        url.setScheme( m_connectionInfo.allowSslConnections ? lit("https") : lit("http") );
        //url.setPath("ec2/events");
        url = nx::utils::Url( url.toString( QUrl::RemovePath | QUrl::RemoveQuery ) + lit("/ec2/events") );
        QUrlQuery q;
        url.setQuery(q);
        m_connectionFactory->messageBus()->addOutgoingConnectionToPeer(m_remotePeerId, url);
    }

    void RemoteEC2Connection::stopReceivingNotifications()
    {
        base_type::stopReceivingNotifications();
        if (m_connectionFactory->messageBus())
        {
            m_connectionFactory->messageBus()->removeOutgoingConnectionFromPeer(m_remotePeerId);
            m_connectionFactory->messageBus()->removeHandler( notificationManager() );
            m_connectionFactory->messageBus()->init(MessageBusType::None);
        }

        //TODO #ak next call can be placed here just because we always have just one connection to EC
        //todo: #singletone it is not true any more
        m_connectionFactory->timeSyncManager()->forgetSynchronizedTime();
    }

    Timestamp RemoteEC2Connection::getTransactionLogTime() const
    {
        NX_ASSERT(true); //< not implemented
        return Timestamp();
    }

    void RemoteEC2Connection::setTransactionLogTime(Timestamp /*value*/)
    {
        NX_ASSERT(true); //< not implemented
    }

}

