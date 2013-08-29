#include "resource_name.h"

#include <QHostAddress>

#include <core/resource/resource.h>

QString extractHost(const QString &url) 
{
    /* speed optimization. This version many times faster. It is important for event log speed */ 
    int prefixIdx = url.indexOf(lit("://"));
    if (prefixIdx == -1)
        return url;
    prefixIdx += 3;
    int postfixIdx = url.indexOf(L':', prefixIdx);
    if (postfixIdx)
        return url.mid(prefixIdx, postfixIdx - prefixIdx);
    else
        return url.mid(prefixIdx);

    /*
    // Try it as a host address first.
    QHostAddress hostAddress(url);
    if(!hostAddress.isNull())
        return hostAddress.toString();

    // Then go default QUrl route. 
    return QUrl(url).host();
    */
}

QString getFullResourceName(const QnResourcePtr &resource, bool showIp) {
    if (!resource)
        return QString();

    QnResource::Flags flags = resource->flags();
    if (showIp && ((flags & QnResource::network) || (flags & QnResource::server && flags & QnResource::remote))) {
        QString host = extractHost(resource->getUrl());
        if(!host.isEmpty())
            return QString(QLatin1String("%1 (%2)")).arg(resource->getName()).arg(host);
    }
    return resource->getName();
}
