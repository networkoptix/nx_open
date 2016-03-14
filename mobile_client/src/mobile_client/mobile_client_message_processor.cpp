#include "mobile_client_message_processor.h"

#include <core/resource/resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <api/app_server_connection.h>
#include <common/common_module.h>
#include <nx/network/socket_common.h>

QnMobileClientMessageProcessor::QnMobileClientMessageProcessor() :
    base_type()
{
}

bool QnMobileClientMessageProcessor::isConnected() const {
    switch (connectionState()) {
    case QnConnectionState::Connected:
    case QnConnectionState::Reconnecting:
    case QnConnectionState::Ready:
        return true;
    default:
        return false;
    }
}

void QnMobileClientMessageProcessor::updateResource(const QnResourcePtr &resource) {
    base_type::updateResource(resource);

    if (resource->getId() == qnCommon->remoteGUID())
        updateMainServerApiUrl();
}

QnResourceFactory* QnMobileClientMessageProcessor::getResourceFactory()
{
    return QnMobileClientCameraFactory::instance();
}

void QnMobileClientMessageProcessor::updateMainServerApiUrl() {
    QnMediaServerResourcePtr server = qnCommon->currentServer();
    if (!server)
        return;

    QUrl url = QnAppServerConnectionFactory::url();
    if (!url.isValid())
        return;

    server->setPrimaryAddress(SocketAddress(url.host(), url.port(0)));
}
