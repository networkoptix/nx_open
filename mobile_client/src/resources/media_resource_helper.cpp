#include "media_resource_helper.h"

#include <QtCore/QUrlQuery>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>
#include <common/common_module.h>

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
    }
}

QUrl QnMediaResourceHelper::mediaUrl() const {
    QnVirtualCameraResourcePtr camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return QUrl();

    QnUuid serverId = qnCommon->remoteGUID();
    QnMediaServerResourcePtr server = qnResPool->getResourceById(serverId).dynamicCast<QnMediaServerResource>();
    if (!server)
        return QUrl();

    QUrl url(server->getUrl());

    url.setUserName(lit("admin"));
    url.setPassword(lit("123"));

    url.setScheme(lit("http"));
    url.setPath(lit("/media/%1.webm").arg(camera->getMAC().toString()));

    QUrlQuery query;
    query.addQueryItem(lit("serverGuid"), serverId.toString());
    query.addQueryItem(lit("cameraGuid"), camera->getId().toString());
    query.addQueryItem(lit("resolution"), lit("360p"));
    url.setQuery(query);

    return url;
}
