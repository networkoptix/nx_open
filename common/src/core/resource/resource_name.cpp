#include "resource_name.h"

#include <QHostAddress>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

QString extractHost(const QString &url) 
{
    /* Don't go through QHostAddress/QUrl constructors as it is SLOW. 
     * Speed is important for event log. */ 
    int startPos = url.indexOf(lit("://"));
    startPos = startPos == -1 ? 0 : startPos + 3;

    int endPos = url.indexOf(L':', startPos);
    if (endPos == -1)
        endPos = url.indexOf(L'/', startPos); /* No port, but we may still get '/' after address. */
    endPos = endPos == -1 ? url.size() : endPos;

    return url.mid(startPos, endPos - startPos);
}

QString getEdgeServerName(const QnResourcePtr &resource) {
    QnResourceList cameras = qnResPool->getResourcesByParentId(resource->getId()).filtered<QnVirtualCameraResource>();
    if (cameras.size() == 1)
        return cameras.first()->getName();
    return resource->getName();
}

QString getFullResourceName(const QnResourcePtr &resource, bool showIp) {
    if (!resource)
        return QString();

    QString baseName = QnMediaServerResource::isEdgeServer(resource)
        ? getEdgeServerName(resource)
        : resource->getName();

    Qn::ResourceFlags flags = resource->flags();
    if (showIp && ((flags & Qn::network) || (flags & Qn::server && flags & Qn::remote))) {
        QString host = extractHost(resource->getUrl());
        if(!host.isEmpty())
            return QString(QLatin1String("%1 (%2)")).arg(baseName).arg(host);
    }
    return baseName;
}
