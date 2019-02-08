/**********************************************************
* 30 jan 2014
* a.kolesnikov
***********************************************************/

#include "remote_ec_connection.h"

#include <api/app_server_connection.h>

#include <transaction/message_bus_adapter.h>
#include <nx/p2p/p2p_message_bus.h>
#include "common/common_module.h"

namespace ec2
{
    RemoteEC2Connection::RemoteEC2Connection(
        nx::vms::api::PeerType peerType,
        const AbstractECConnectionFactory* connectionFactory,
        const QnUuid& remotePeerId,
        const FixedUrlClientQueryProcessorPtr& queryProcessor,
        const QnConnectionInfo& connectionInfo )
        :
        base_type(connectionFactory, queryProcessor.get() ),
        m_peerType(peerType),
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
        m_queryProcessor->setUrl(url);
    }

    void RemoteEC2Connection::startReceivingNotifications()
    {
        if (m_connectionInfo.p2pMode)
            m_connectionFactory->messageBus()->init<nx::p2p::MessageBus>(m_peerType);
        else
            m_connectionFactory->messageBus()->init<ec2::QnTransactionMessageBus>(m_peerType);
        m_connectionFactory->messageBus()->setHandler(notificationManager());

        base_type::startReceivingNotifications();

        nx::utils::Url url(m_queryProcessor->getUrl());
        url.setScheme(nx::network::http::urlSheme(m_connectionInfo.allowSslConnections));
        url = nx::utils::Url( url.toString( QUrl::RemovePath | QUrl::RemoveQuery ));
        QUrlQuery q;
        url.setQuery(q);
        m_connectionFactory->messageBus()->addOutgoingConnectionToPeer(
            m_remotePeerId, nx::vms::api::PeerType::server, url);
    }

    void RemoteEC2Connection::stopReceivingNotifications()
    {
        base_type::stopReceivingNotifications();
        if (m_connectionFactory->messageBus())
        {
            m_connectionFactory->messageBus()->removeOutgoingConnectionFromPeer(m_remotePeerId);
            m_connectionFactory->messageBus()->removeHandler( notificationManager() );
            m_connectionFactory->messageBus()->reset();
        }
    }

    nx::vms::api::Timestamp RemoteEC2Connection::getTransactionLogTime() const
    {
        NX_ASSERT(true); //< not implemented
        return {};
    }

    void RemoteEC2Connection::setTransactionLogTime(nx::vms::api::Timestamp /*value*/)
    {
        NX_ASSERT(true); //< not implemented
    }

}

