#include "mobile_client_message_processor.h"

#include "api/app_server_connection.h"
#include "common/common_module.h"

QnMobileClientMessageProcessor::QnMobileClientMessageProcessor() :
    QnCommonMessageProcessor()
{
}

void QnMobileClientMessageProcessor::init(const ec2::AbstractECConnectionPtr &connection) {
    QnCommonMessageProcessor::init(connection);
    if (connection) {
        assert(!m_connected);
        assert(qnCommon->remoteGUID().isNull());
        qnCommon->setRemoteGUID(connection->connectionInfo().ecsGuid);
        connect(connection.get(),   &ec2::AbstractECConnection::remotePeerFound,    this,   &QnMobileClientMessageProcessor::at_remotePeerFound);
        connect(connection.get(),   &ec2::AbstractECConnection::remotePeerLost,     this,   &QnMobileClientMessageProcessor::at_remotePeerLost);
    } else if (m_connected) { // double init by null is allowed
        assert(!qnCommon->remoteGUID().isNull());
        ec2::ApiPeerAliveData data;
        data.peer.id = qnCommon->remoteGUID();
        qnCommon->setRemoteGUID(QUuid());
        m_connected = false;
        emit connectionClosed();
    } else if (!qnCommon->remoteGUID().isNull()) { // we are trying to reconnect to server now
        qnCommon->setRemoteGUID(QUuid());
    }
}

void QnMobileClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
}

void QnMobileClientMessageProcessor::onResourceStatusChanged(const QnResourcePtr &resource, QnResource::Status status) {
}

void QnMobileClientMessageProcessor::at_remotePeerFound(ec2::ApiPeerAliveData data, bool isProxy) {
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerFound received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;

    assert(!isProxy);
    assert(!m_connected);

    m_connected = true;
    emit connectionOpened();
}

void QnMobileClientMessageProcessor::at_remotePeerLost(ec2::ApiPeerAliveData data, bool isProxy) {
    if (qnCommon->remoteGUID().isNull()) {
        qWarning() << "at_remotePeerLost received while disconnected";
        return;
    }

    if (data.peer.id != qnCommon->remoteGUID())
        return;


    Q_ASSERT_X(!isProxy, Q_FUNC_INFO, "!isProxy");
    Q_ASSERT_X(m_connected, Q_FUNC_INFO, "m_connected");

    m_connected = false;
    emit connectionClosed();
}
