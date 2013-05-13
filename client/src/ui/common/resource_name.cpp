#include "resource_name.h"

#include <QHostAddress>

#include <core/resource/resource.h>
#include <utils/settings.h>

QString extractHost(const QString &url) {
    /* Try it as a host address first. */
    QHostAddress hostAddress(url);
    if(!hostAddress.isNull())
        return hostAddress.toString();

    /* Then go default QUrl route. */
    return QUrl(url).host();
}

QString getResourceName(const QnResourcePtr& resource) {
    if (!resource)
        return QString();

    QnResource::Flags flags = resource->flags();
    if (qnSettings->isIpShownInTree()) {
        if((flags & QnResource::network) || (flags & QnResource::server && flags & QnResource::remote)) {
            QString host = extractHost(resource->getUrl());
            if(!host.isEmpty())
                return QString(QLatin1String("%1 (%2)")).arg(resource->getName()).arg(host);
        }
    }
    return resource->getName();
}
