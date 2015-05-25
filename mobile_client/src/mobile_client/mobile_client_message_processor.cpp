#include "mobile_client_message_processor.h"

#include "core/resource/resource.h"
#include "core/resource_management/resource_pool.h"
#include "api/app_server_connection.h"
#include "common/common_module.h"

QnMobileClientMessageProcessor::QnMobileClientMessageProcessor() :
    base_type(),
    m_connected(false)
{
}

void QnMobileClientMessageProcessor::init(const ec2::AbstractECConnectionPtr &connection) {
    QnCommonMessageProcessor::init(connection);
    if (connection) {
        assert(!m_connected);
        assert(qnCommon->remoteGUID().isNull());
        qnCommon->setRemoteGUID(connection->connectionInfo().ecsGuid);
        connect(connection,     &ec2::AbstractECConnection::remotePeerFound,    this,   &QnMobileClientMessageProcessor::at_remotePeerFound);
        connect(connection,     &ec2::AbstractECConnection::remotePeerLost,     this,   &QnMobileClientMessageProcessor::at_remotePeerLost);
        connect(connection->getMiscManager(), &ec2::AbstractMiscManager::systemNameChangeRequested,
                this, &QnMobileClientMessageProcessor::at_systemNameChangeRequested);
    } else if (m_connected) { // double init by null is allowed
        assert(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QnUuid());
        m_connected = false;
        emit connectionClosed();
    } else if (!qnCommon->remoteGUID().isNull()) { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QnUuid());
    }
}

void QnMobileClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
    QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId());

    if (ownResource)
        ownResource->update(resource);
    else
        qnResPool->addResource(resource);
}

void QnMobileClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, Qn::ResourceStatus status) {
    resource->setStatus(status);
}

void QnMobileClientMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data) {
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    assert(!m_connected);

    m_connected = true;
    emit connectionOpened();
}

void QnMobileClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data) {
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerLost received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;


    Q_ASSERT_X(m_connected, Q_FUNC_INFO, "m_connected");

    m_connected = false;
    emit connectionClosed();
}

void QnMobileClientMessageProcessor::at_systemNameChangeRequested(const QString &systemName) {
    qnCommon->setLocalSystemName(systemName);
}
