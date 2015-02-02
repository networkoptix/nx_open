#include "media_resource_helper.h"

#include <QtCore/QUrlQuery>

#include "api/app_server_connection.h"
#include "api/network_proxy_factory.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "core/resource/camera_resource.h"
#include "common/common_module.h"

QnMediaResourceHelper::QnMediaResourceHelper(QObject *parent) :
    QObject(parent)
{
}

QString QnMediaResourceHelper::resourceId() const {
    if (!m_resource)
        return QString();
    return m_resource->getId().toString();
}

void QnMediaResourceHelper::setResourceId(const QString &id) {
    QnUuid uuid(id);

    QnResourcePtr resource = qnResPool->getResourceById(uuid);
    if (resource != m_resource) {
        m_resource = resource;
        emit resourceIdChanged();
        emit resourceNameChanged();
        emit mediaUrlChanged();
    }
}

QUrl QnMediaResourceHelper::mediaUrl() const {
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return QUrl();

    QnMediaServerResourcePtr server = camera->getParentResource().dynamicCast<QnMediaServerResource>();
    if (!server)
        return QUrl();

    QUrl url(server->getUrl());

    url.setUserName(QnAppServerConnectionFactory::url().userName());
    url.setPassword(QnAppServerConnectionFactory::getConnection2()->authInfo());

    url.setScheme(lit("http"));
    url.setPath(lit("/media/%1.webm").arg(camera->getMAC().toString()));

    QUrlQuery query;
    query.addQueryItem(lit("serverGuid"), server->getId().toString());
    query.addQueryItem(lit("cameraGuid"), camera->getId().toString());
    query.addQueryItem(lit("resolution"), lit("360p"));
    url.setQuery(query);

    return QnNetworkProxyFactory::instance()->urlToResource(url, server);
}

QString QnMediaResourceHelper::resourceName() const {
    if (!m_resource)
        return QString();

    return m_resource->getName();
}
