#include "resource_name.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

namespace
{
    void getResourceDisplayInformation(const QnResourcePtr& resource, bool queryHost, QString& name, QString& host)
    {
        if (!resource)
        {
            name = QString();
            host = QString();
            return;
        }

        name = resource->getName();
        Qn::ResourceFlags flags = resource->flags();

        if (flags.testFlag(Qn::live_cam)) /* Quick check */
            if (QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>())
                name = camera->getUserDefinedName();

        if (queryHost && ((flags & Qn::network) || (flags & Qn::server && flags & Qn::remote)))
            host = extractHost(resource->getUrl());
        else
            host = QString();
    }
};

QString extractHost(const QString& url)
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

void getResourceDisplayInformation(const QnResourcePtr& resource, QString& name, QString& host)
{
    getResourceDisplayInformation(resource, true, name, host);
}

QString getFullResourceName(const QnResourcePtr& resource, bool showIp)
{
    if (!resource)
        return QString();

    QString baseName, host;
    getResourceDisplayInformation(resource, showIp, baseName, host);

    if (!host.isEmpty())
        return QString(lit("%1 (%2)")).arg(baseName, host);

    return baseName;
}
