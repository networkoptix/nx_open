// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "other_server_display_info.h"

#include <nx/vms/api/data/module_information.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>

namespace {

const QString kFormatTemplate = "%1 (%2)";

} // namespace

namespace nx::vms::client::desktop {

OtherServerDisplayInfo::OtherServerDisplayInfo(
    const QnUuid& serverId,
    const OtherServersManager* otherServersManager)
    :
    m_otherServerManager(otherServersManager),
    m_serverId(serverId)
{
}

QString OtherServerDisplayInfo::name() const
{
    return m_otherServerManager->getModuleInformationWithAddresses(m_serverId).name;
}

QString OtherServerDisplayInfo::host() const
{
    return m_otherServerManager->getUrl(m_serverId).host();
}

int OtherServerDisplayInfo::port() const
{
    return m_otherServerManager->getUrl(m_serverId).port();
}

QString OtherServerDisplayInfo::extraInfo() const
{
    return host();
}

QString OtherServerDisplayInfo::toString(Qn::ResourceInfoLevel detailLevel) const
{
    switch (detailLevel)
    {
        case Qn::RI_NameOnly:
        {
            return name();
        }
        case Qn::RI_WithUrl:
        {
            const auto host = this->host();
            const auto name = this->name();
            return host.isEmpty() ? name : kFormatTemplate.arg(name, host);
        }
        case Qn::RI_FullInfo:
        {
            return kFormatTemplate.arg(name(), extraInfo());
        }
        default:
        {
            NX_ASSERT(false, "Unexpected detail level");
            break;
        }
    }
    return QString();
}

} // namespace nx::vms::client::desktop
