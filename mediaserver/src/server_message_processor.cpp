#include "server_message_processor.h"

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <qglobal.h>

#include <api/message_source.h>
#include <api/app_server_connection.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_discovery_manager.h>
#include <core/resource_management/resource_pool.h>

#include <recorder/recording_manager.h>

#include "serverutil.h"
#include "settings.h"

#include "utils/network/simple_http_client.h"

QnServerMessageProcessor::QnServerMessageProcessor():
    base_type() {
    m_tryDirectConnect = true;
}

void QnServerMessageProcessor::handleConnectionOpened(const QnMessage &message) {
    m_tryDirectConnect = true;
    base_type::handleConnectionOpened(message);
}

void QnServerMessageProcessor::handleConnectionClosed(const QString &errorString) {
    // update EC port
    int port = MSSettings::roSettings()->value("appserverPort", DEFAULT_APPSERVER_PORT).toInt(); // defaulting to proxy

    QUrl url = QnAppServerConnectionFactory::defaultUrl();
    url.setPort(port);
    QnAppServerConnectionFactory::setDefaultUrl(url);

    // check if it proxy connection and direct EC access is available
    if (m_tryDirectConnect) {
        m_tryDirectConnect = false;
        QAuthenticator auth;
        auth.setUser(url.userName());
        auth.setPassword(url.password());
        static const int TEST_DIRECT_CONNECT_TIMEOUT = 2000;
        CLSimpleHTTPClient testClient(url.host(), port, TEST_DIRECT_CONNECT_TIMEOUT, auth);
        CLHttpStatus result = testClient.doGET(lit("proxy_api/ec_port"));
        if (result == CL_HTTP_SUCCESS)
        {
            QUrl directURL;
            QByteArray data;
            testClient.readAll(data);
            directURL = url;
            directURL.setPort(data.toInt());
            QnAppServerConnectionFactory::setDefaultUrl(directURL);
        }
    }
    base_type::handleConnectionClosed(errorString);
}

void QnServerMessageProcessor::loadRuntimeInfo(const QnMessage &message) {
    base_type::loadRuntimeInfo(message);
    QnAppServerConnectionFactory::setAllowCameraChanges(message.allowCameraChanges);
}

void QnServerMessageProcessor::handleMessage(const QnMessage &message) {
    base_type::handleMessage(message);

    NX_LOG( QString::fromLatin1("Received message %1, resourceId %2, resource %3").
            arg(Qn::toString(message.messageType)).arg(message.resourceId.toString()).arg(message.resource ? message.resource->getName() : QString("NULL")), cl_logDEBUG1 );

    switch (message.messageType) {
    case Qn::Message_Type_License: {
        // New license added. LicensePool verifies it.
        qnLicensePool->addLicense(message.license);
        break;
    }
    case Qn::Message_Type_CameraServerItem: {
        QnCameraHistoryPool::instance()->addCameraHistoryItem(*message.cameraServerItem);
        break;
    }
    case Qn::Message_Type_ResourceChange: {
        QnResourcePtr resource = message.resource;

        QnMediaServerResourcePtr ownMediaServer = qnResPool->getResourceByGuid(serverGuid()).dynamicCast<QnMediaServerResource>();

        bool isServer = resource.dynamicCast<QnMediaServerResource>();
        bool isCamera = resource.dynamicCast<QnVirtualCameraResource>();
        bool isUser = resource.dynamicCast<QnUserResource>();

        if (!isServer && !isCamera && !isUser)
            return;

        // If the resource is mediaServer then ignore if not this server
        if (isServer && resource->getGuid() != serverGuid())
            return;

        //storing all servers' cameras too
        // If camera from other server - marking it
        if (isCamera && resource->getParentId() != ownMediaServer->getId())
            resource->addFlags( QnResource::foreigner );

        // We are always online
        if (isServer)
            resource->setStatus(QnResource::Online);

        if (QnResourcePtr ownResource = qnResPool->getResourceById(resource->getId(), QnResourcePool::AllResources))
            ownResource->update(resource);
        else
            qnResPool->addResource(resource);

        if (isServer)
            syncStoragesToSettings(ownMediaServer);
        break;
    }
    case Qn::Message_Type_ResourceDisabledChange: {
        //ignoring messages for foreign resources
        if (QnResourcePtr resource = qnResPool->getResourceById(message.resourceId)) {
            resource->setDisabled(message.resourceDisabled);
            if (message.resourceDisabled) // we always ignore status changes
                resource->setStatus(QnResource::Offline);
        }
        break;
    }
    case Qn::Message_Type_ResourceDelete: {
        if (QnResourcePtr resource = qnResPool->getResourceById(message.resourceId, QnResourcePool::AllResources))
            qnResPool->removeResource(resource);
        break;
    }
    default:
        break;
    }


}
