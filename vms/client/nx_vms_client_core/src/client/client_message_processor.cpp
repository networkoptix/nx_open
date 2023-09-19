// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_message_processor.h"

#include <QtCore/QMetaEnum>

#include <client_core/client_core_module.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_global.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/peer_alive_data.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/core/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/rules/engine_holder.h>
#include <nx_ec/abstract_ec_connection.h>
#include <nx_ec/managers/abstract_misc_manager.h>
#include <nx_ec/managers/abstract_showreel_manager.h>
#include <utils/common/synctime.h>

using namespace nx::vms::client::core;

QnClientMessageProcessor::QnClientMessageProcessor(
    nx::vms::common::SystemContext* context,
    QObject* parent)
    :
    base_type(context, parent)
{
}

void QnClientMessageProcessor::init(const ec2::AbstractECConnectionPtr& connection)
{
    if (connection)
    {
        NX_VERBOSE(this, "%1() - connecting to %2", __func__, connection->address());

        if (m_status.state() != QnConnectionState::Reconnecting)
        {
            NX_DEBUG(this, "init, state -> Connecting");
            m_status.setState(QnConnectionState::Connecting);
        }
    }
    else
    {
        NX_DEBUG(this, lit("init nullptr, state -> Disconnected"));
        m_status.setState(QnConnectionState::Disconnected);
    }

    if (m_connection)
        NX_VERBOSE(this, "%1() - clearing existing connection to %2", __func__, m_connection->address());

    if (connection)
    {
        auto info = connection->moduleInformation();
        const auto serverId = info.id;
        NX_DEBUG(this, "Connection established to %1", serverId);
    }
    else if (m_connected)
    {
        // Double init by null is allowed.
        m_connected = false;
        if (m_policy == HoldConnectionPolicy::none)
            holdConnection(HoldConnectionPolicy::none);

        NX_DEBUG(this, "Deinit while connected, connection closed");
        emit connectionClosed();
    }

    QnCommonMessageProcessor::init(connection);
}

const QnClientConnectionStatus* QnClientMessageProcessor::connectionStatus() const
{
    return &m_status;
}

void QnClientMessageProcessor::holdConnection(HoldConnectionPolicy policy)
{
    NX_DEBUG(this, nx::format("Hold connection set to %1")
        .arg(QMetaEnum::fromType<HoldConnectionPolicy>().valueToKey(int(policy))));
    if (m_policy == policy)
        return;

    m_policy = policy;

    if (m_policy == HoldConnectionPolicy::none && !m_connected
        && connection()->moduleInformation().id.isNull())
    {
        NX_DEBUG(this, "Unholding connection while disconnected, connection closed");
        emit connectionClosed();
    }
}

Qt::ConnectionType QnClientMessageProcessor::handlerConnectionType() const
{
    return Qt::QueuedConnection;
}

void QnClientMessageProcessor::disconnectFromConnection(const ec2::AbstractECConnectionPtr& connection)
{
    m_policy = HoldConnectionPolicy::none;
    base_type::disconnectFromConnection(connection);
    connection->miscNotificationManager()->disconnect(this);
}

void QnClientMessageProcessor::onResourceStatusChanged(
    const QnResourcePtr &resource,
    nx::vms::api::ResourceStatus status,
    ec2::NotificationSource /*source*/)
{
    resource->setStatus(status);
}

void QnClientMessageProcessor::updateResource(
    const QnResourcePtr& resource,
    ec2::NotificationSource source)
{
    // In some rare cases we can receive updateResource call when the client is in the reconnect
    // process (it starts an event loop inside the main thread). Populating the resource pool or
    // changing resources is highly inappropriate at that time.
    if (!m_connection || !NX_ASSERT(resource))
        return;

    resource->addFlags(Qn::remote);
    resource->removeFlags(Qn::local);

    base_type::updateResource(resource, source);

    if (QnResourcePtr ownResource = resourcePool()->getResourceById(resource->getId()))
        ownResource->update(resource);
    else
        resourcePool()->addResource(resource);
}

void QnClientMessageProcessor::handleRemotePeerFound(QnUuid peer, nx::vms::api::PeerType peerType)
{
    base_type::handleRemotePeerFound(peer, peerType);

    /*
     * Avoiding multiple connectionOpened() if client was on breakpoint or
     * if ec connection settings were changed.
     */
    if (m_connected)
        return;

    if (!NX_ASSERT(connection()))
        return;

    if (connection()->moduleInformation().id.isNull())
    {
        NX_WARNING(this, "Remote peer found while disconnected");
        return;
    }

    if (peer != connection()->moduleInformation().id)
        return;

    NX_DEBUG(this, lit("peer found, state -> Connected"));
    m_status.setState(QnConnectionState::Connected);
    m_connected = true;
    holdConnection(HoldConnectionPolicy::none);

    NX_DEBUG(this, "Remote peer found, connection opened");
    emit connectionOpened();
}

void QnClientMessageProcessor::handleRemotePeerLost(QnUuid peer, nx::vms::api::PeerType peerType)
{
    base_type::handleRemotePeerLost(peer, peerType);

    // RemotePeerLost signal during connect is perfectly OK. We are receiving old notifications
    // that were not sent as TransactionMessageBus was stopped. Peer id is the same if we are
    // connecting to the same server we are already connected to (and just disconnected).
    if (!m_connected)
        return;

    const QnUuid currentPeer = connection()
        ? connection()->moduleInformation().id
        : QnUuid();

    if (currentPeer.isNull())
    {
        NX_WARNING(this, "Remote peer lost while disconnected");
        return;
    }

    if (peer != currentPeer)
        return;

    NX_DEBUG(this, lit("peer lost, state -> Reconnecting"));
    m_status.setState(QnConnectionState::Reconnecting);

    auto currentServer = resourcePool()->getResourceById<QnMediaServerResource>(
        connection()->moduleInformation().id);

    /* Mark server as offline, so user will understand why is he reconnecting. */
    if (currentServer && m_policy != HoldConnectionPolicy::reauth)
        currentServer->setStatus(nx::vms::api::ResourceStatus::offline);

    m_connected = false;

    if (m_policy == HoldConnectionPolicy::none)
    {
        NX_DEBUG(this, "Connection closed");
        emit connectionClosed();
    }
    else
    {
        NX_DEBUG(this, "Holding connection...");
    }
}

void QnClientMessageProcessor::removeHardwareIdMapping(const QnUuid& id)
{
    emit hardwareIdMappingRemoved(id);
}

void QnClientMessageProcessor::onGotInitialNotification(const nx::vms::api::FullInfoData& fullData)
{
    if (m_status.state() != QnConnectionState::Connected &&
        NX_ASSERT(m_status.state() != QnConnectionState::Ready))
    {
        // Skip belated initial resources notification received after disconnection.
        // Pass double initial resources notification with an assertion failure.
        return;
    }

    base_type::onGotInitialNotification(fullData);
    m_status.setState(QnConnectionState::Ready);

    auto currentServer = resourcePool()->getResourceById<QnMediaServerResource>(
        connection()->moduleInformation().id);

    NX_ASSERT(currentServer);

    /* Get server time as soon as we setup connection. */
    qnSyncTime->resync();
}
