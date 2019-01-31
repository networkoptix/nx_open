#include "resource_display_info.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource_management/user_roles_manager.h>
#include <common/common_module.h>

#include <nx/network/address_resolver.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>

namespace {

enum Priority { kDns, kLocalIpV4, kRemoteIp, kLocalIpV6, kLocalHost, kCloud };

Priority hostPriority(const nx::network::HostAddress& host)
{
    if (host.isLocalHost())
        return kLocalHost;

    if (host.isLocalNetwork())
        return host.ipV4() ? kLocalIpV4 : kLocalIpV6;

    if (host.ipV4() || (bool) host.ipV6().first)
        return kRemoteIp;

    if (nx::network::SocketGlobals::addressResolver().isCloudHostName(host.toString()))
        return kCloud;

    return kDns;
};

nx::network::SocketAddress getServerUrl(const QnMediaServerResourcePtr& server)
{
    NX_ASSERT(server);
    if (!server)
        return QString();

    // We should not display localhost or cloud addresses to the user.
    auto primaryUrl = server->getPrimaryAddress();
    if (hostPriority(primaryUrl.address) < kLocalHost)
        return primaryUrl;

    auto allAddresses = server->getAllAvailableAddresses();
    return *std::min_element(allAddresses.cbegin(), allAddresses.cend(),
        [](const auto& l, const auto& r)
        {
            return hostPriority(l.address) < hostPriority(r.address);
        });
}

const QString kFormatTemplate = "%1 (%2)";

} // namespace

QnResourceDisplayInfo::QnResourceDisplayInfo(const QnResourcePtr& resource):
    m_resource(resource)
{
}

QString QnResourceDisplayInfo::name() const
{
    ensureConstructed(Qn::RI_NameOnly);
    return m_name;
}

QString QnResourceDisplayInfo::host() const
{
    ensureConstructed(Qn::RI_WithUrl);
    return m_host;
}

int QnResourceDisplayInfo::port() const
{
    ensureConstructed(Qn::RI_WithUrl);
    return m_port;
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
            if (m_host.isEmpty())
                return m_name;
            return kFormatTemplate.arg(m_name, m_host);
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
            if (const auto camera = m_resource.dynamicCast<QnSecurityCamResource>())
                m_name = camera->getUserDefinedName();
        }

        if (const auto storage = m_resource.dynamicCast<QnStorageResource>())
            m_name = storage->getUrl();
    }

    if (detailLevel == Qn::RI_NameOnly)
        return;

    if (m_host.isEmpty())
    {
        if (flags.testFlag(Qn::remote_server))
        {
            auto url = getServerUrl(m_resource.dynamicCast<QnMediaServerResource>());
            m_host = url.address.toString();
            m_port = url.port;
        }
        else if (flags.testFlag(Qn::network))
        {
            if (const auto networkResource = m_resource.dynamicCast<QnNetworkResource>())
                m_host = networkResource->getHostAddress();
        }
    }

    if (detailLevel == Qn::RI_WithUrl)
        return;

    if (flags.testFlag(Qn::user))
    {
        if (const auto user = m_resource.dynamicCast<QnUserResource>())
        {
            if (const auto common = user->commonModule())
                m_extraInfo = common->userRolesManager()->userRoleName(user);
        }
    }
    else
    {
        m_extraInfo = m_host;
    }
}

