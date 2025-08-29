// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "tier_usage_common.h"

#include <QtCore/QCoreApplication>

#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop::saas {

class TierUsageStrings
{
    Q_DECLARE_TR_FUNCTIONS(TierUsageStrings)

public:
    static QString tierLimitTypeToString(nx::vms::api::SaasTierLimitName limitType)
    {
        switch (limitType)
        {
            case nx::vms::api::SaasTierLimitName::maxServersPerSite:
                return tr("Number of servers per Site");

            case nx::vms::api::SaasTierLimitName::maxDevicesPerServer:
                return tr("Number of devices per server");

            case nx::vms::api::SaasTierLimitName::maxItemsInLayout:
                return tr("Number of items on layout");

            case nx::vms::api::SaasTierLimitName::maxDaysArchiveLocal:
                return tr("Number of days of archive");

            case nx::vms::api::SaasTierLimitName::ldapAllowed:
                return tr("LDAP");

            case nx::vms::api::SaasTierLimitName::videoWallAllowed:
                return tr("Video Wall");

            case nx::vms::api::SaasTierLimitName::crossSiteAllowed:
                return tr("Cross-Site features");

            default:
                NX_ASSERT("Unexpected tier limit type");
        }

        return {};
    }
};

bool isBooleanTierLimitType(nx::vms::api::SaasTierLimitName limitType)
{
    switch (limitType)
    {
        case nx::vms::api::SaasTierLimitName::maxServersPerSite:
        case nx::vms::api::SaasTierLimitName::maxDevicesPerServer:
        case nx::vms::api::SaasTierLimitName::maxItemsInLayout:
        case nx::vms::api::SaasTierLimitName::maxDaysArchiveLocal:
            return false;

        case nx::vms::api::SaasTierLimitName::ldapAllowed:
        case nx::vms::api::SaasTierLimitName::videoWallAllowed:
        case nx::vms::api::SaasTierLimitName::crossSiteAllowed:
            return true;

    }

    NX_ASSERT("Unexpected tier limit type %1", static_cast<int>(limitType));
    return false;
}

QString tierLimitTypeToString(nx::vms::api::SaasTierLimitName limitType)
{
    return TierUsageStrings::tierLimitTypeToString(limitType);
}

} // namespace nx::vms::client::desktop::saas
