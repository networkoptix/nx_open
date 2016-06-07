#include "resource_display_info.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/device_dependent_strings.h>

namespace
{
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

    const QString kFormatTemplate = lit("%1 (%2)");
};



QnResourceDisplayInfo::QnResourceDisplayInfo(const QnResourcePtr& resource) :
    m_resource(resource),
    m_detailLevel(Qn::RI_Invalid),
    m_name(),
    m_url(),
    m_extraInfo()
{}

QString QnResourceDisplayInfo::name() const
{
    ensureConstructed(Qn::RI_NameOnly);
    return m_name;
}

QString QnResourceDisplayInfo::url() const
{
    ensureConstructed(Qn::RI_WithUrl);
    return m_url;
}

QString QnResourceDisplayInfo::extraInfo() const
{
    ensureConstructed(Qn::RI_FullInfo);
    return m_extraInfo;
}

QString QnResourceDisplayInfo::toString(Qn::ResourceInfoLevel detailLevel) const
{
    ensureConstructed(detailLevel);
    switch (detailLevel)
    {
        case Qn::RI_NameOnly:
            return m_name;
        case Qn::RI_WithUrl:
        {
            if (m_url.isEmpty())
                return m_name;
            return kFormatTemplate.arg(m_name, m_url);
        }
        case Qn::RI_FullInfo:
        {
            if (m_extraInfo.isEmpty())
                return m_name;
            return kFormatTemplate.arg(m_name, m_extraInfo);
        }
        default:
            break;
    }
    return QString();
}

void QnResourceDisplayInfo::ensureConstructed(Qn::ResourceInfoLevel detailLevel) const
{
    if (detailLevel <= m_detailLevel)
        return;

    if (!m_resource)
        return;

    m_detailLevel = detailLevel;

    Qn::ResourceFlags flags = m_resource->flags();

    /* Fast check in case of higher-level request. */
    if (m_name.isEmpty())
    {
        m_name = m_resource->getName();
        if (m_resource->hasFlags(Qn::live_cam)) /* Quick check */
        {
            if (QnSecurityCamResourcePtr camera = m_resource.dynamicCast<QnSecurityCamResource>())
                m_name = camera->getUserDefinedName();
        }
    }

    if (detailLevel == Qn::RI_NameOnly)
        return;

    if (m_url.isEmpty())
    {
        if ((flags.testFlag(Qn::network) || flags.testFlag(Qn::remote_server)))
            m_url = extractHost(m_resource->getUrl());
    }

    if (detailLevel == Qn::RI_WithUrl)
        return;

    if (flags.testFlag(Qn::user))
    {
        if (const QnUserResourcePtr& user = m_resource.dynamicCast<QnUserResource>())
            m_extraInfo = user->fullName();
    }
    else
    {
        m_extraInfo = m_url;
    }
}

