// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_display_info.h"

#include <QtNetwork/QHostAddress>

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <nx/network/address_resolver.h>
#include <nx/network/socket_common.h>
#include <nx/network/socket_global.h>
#include <nx/vms/common/resource/server_host_priority.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/user_management/user_management_helpers.h>

using namespace nx::vms::common;

namespace {

nx::network::SocketAddress getServerUrl(const QnMediaServerResourcePtr& server)
{
    NX_ASSERT(server);
    if (!server)
        return nx::network::SocketAddress();

    // We should not display localhost or cloud addresses to the user.
    auto primaryUrl = server->getPrimaryAddress();
    if (serverHostPriority(primaryUrl.address) < ServerHostPriority::localHost)
        return primaryUrl;

    auto allAddresses = server->getAllAvailableAddresses();
    if (allAddresses.empty())
        return primaryUrl;

    return *std::min_element(allAddresses.cbegin(), allAddresses.cend(),
        [](const auto& l, const auto& r)
        {
            return serverHostPriority(l.address) < serverHostPriority(r.address);
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
            m_host = url.address.toString().c_str();
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
            const auto names = nx::vms::common::userGroupNames(user);
            switch (names.size())
            {
                case 0:
                    m_extraInfo = QnUserResource::tr("Custom");
                    break;

                case 1:
                    m_extraInfo = names.front();
                    break;

                default:
                    m_extraInfo = nx::format("%1 (%2)", QnUserResource::tr("Multiple groups"),
                        names.size());
                    break;
            }
        }
    }
    else
    {
        m_extraInfo = m_host;
    }
}
