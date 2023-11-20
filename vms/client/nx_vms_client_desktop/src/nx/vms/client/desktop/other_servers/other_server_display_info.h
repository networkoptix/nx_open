// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <common/common_globals.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class OtherServersManager;

/**
 * OtherServerDisplayInfo imitates QnResourceDisplayInfo behavior which is used for regular
 * resource tree node info displaying.
 */

class NX_VMS_CLIENT_DESKTOP_API OtherServerDisplayInfo
{
public:
    OtherServerDisplayInfo(const QnUuid& serverId, const OtherServersManager* otherServersManager);

    QString name() const;
    QString host() const;
    int port() const;
    QString extraInfo() const;

    QString toString(Qn::ResourceInfoLevel detailLevel) const;

private:
    const OtherServersManager* m_otherServerManager;

    const QnUuid m_serverId;

    /** How detailed the info should be constructed. */
    mutable Qn::ResourceInfoLevel m_detailLevel = Qn::RI_Invalid;
};

} // namespace nx::vms::client::desktop
