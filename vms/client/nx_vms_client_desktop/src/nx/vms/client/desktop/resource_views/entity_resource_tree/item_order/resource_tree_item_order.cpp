// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_tree_item_order.h"

#include <client/client_globals.h>
#include <common/common_globals.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/abstract_item.h>
#include <nx/vms/client/desktop/style/resource_icon_cache.h>
#include <nx/vms/common/intercom/utils.h>

namespace nx::vms::client::desktop {
namespace entity_resource_tree {

using namespace entity_item_model;

ItemOrder serversOrder()
{
    using namespace nx::vms::client::core;

    return {
        [mainOrder = numericOrder()](const AbstractItem* lhs,
            const AbstractItem* rhs)
        {
            const auto lhsStatus = lhs->data(ResourceStatusRole).value<nx::vms::api::ResourceStatus>();
            const auto rhsStatus = rhs->data(ResourceStatusRole).value<nx::vms::api::ResourceStatus>();

            const bool lhsIncompatible = lhsStatus == nx::vms::api::ResourceStatus::incompatible;
            const bool rhsIncompatible = rhsStatus == nx::vms::api::ResourceStatus::incompatible;

            if (lhsIncompatible != rhsIncompatible)
                return rhsIncompatible;

            return mainOrder.comp(lhs, rhs);
        },
        {Qt::DisplayRole, ResourceStatusRole}
    };
}

ItemOrder layoutsOrder()
{
    using namespace nx::vms::common;

    return {
        [mainOrder = numericOrder()] (const AbstractItem* lhs,
            const AbstractItem* rhs)
        {
            const auto lhsResource = lhs->data(core::ResourceRole).value<QnResourcePtr>();
            const auto rhsResource = rhs->data(core::ResourceRole).value<QnResourcePtr>();
            const auto lhsLayout = lhsResource.objectCast<LayoutResource>();
            const auto rhsLayout = rhsResource.objectCast<LayoutResource>();

            const bool lhsIsSharedLayout = lhsLayout->isShared();
            const bool rhsIsSharedLayout = rhsLayout->isShared();


            // Shared layouts are located first.
            if (lhsIsSharedLayout != rhsIsSharedLayout)
                return lhsIsSharedLayout;

            const bool lhsIsIntercomLayout = isIntercomLayout(lhsLayout);
            const bool rhsIsIntercomLayout = isIntercomLayout(rhsLayout);

            // Next are intercom layouts.
            if (lhsIsIntercomLayout != rhsIsIntercomLayout)
                return lhsIsIntercomLayout;

            const bool lhsIsCloudLayout = lhsLayout->isCrossSystem();
            const bool rhsIsCloudLayout = rhsLayout->isCrossSystem();

            // Next are plain layouts, cross system layouts located at the end of the list.
            if (lhsIsCloudLayout != rhsIsCloudLayout)
                return !lhsIsCloudLayout;

            return mainOrder.comp(lhs, rhs);
        },
        {Qt::DisplayRole, Qn::ResourceIconKeyRole}
    };
}

ItemOrder layoutItemsOrder()
{
    const auto groupIndex =
        [](const Qn::ResourceFlags& flags, int iconKey)
        {
            if (flags.testFlag(Qn::web_page))
            {
                if (ini().webPagesAndIntegrations)
                {
                    switch (iconKey)
                    {
                        case QnResourceIconCache::IntegrationProxied: return 2;
                        case QnResourceIconCache::Integration: return 3;
                        case QnResourceIconCache::WebPageProxied: return 4;
                        case QnResourceIconCache::WebPage: return 5;
                    }

                    NX_ASSERT(false, "Unexpected value (%1)", iconKey);
                }

                return iconKey == QnResourceIconCache::WebPageProxied ? 2 : 3;
            }

            if (flags.testFlag(Qn::server))
                return 1;

            return 0;
        };

    return {
        [mainOrder = numericOrder(), groupIndex](const AbstractItem* lhs,
            const AbstractItem* rhs)
        {
            const auto lhsFlags = lhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
            const auto lhsIconKey = lhs->data(Qn::ResourceIconKeyRole).toInt();
            const auto rhsFlags = rhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
            const auto rhsIconKey = rhs->data(Qn::ResourceIconKeyRole).toInt();

            const auto lhsGroupIndex = groupIndex(lhsFlags, lhsIconKey);
            const auto rhsGroupIndex = groupIndex(rhsFlags, rhsIconKey);

            if (lhsGroupIndex == rhsGroupIndex)
                return mainOrder.comp(lhs, rhs);

            return lhsGroupIndex < rhsGroupIndex;

        },
        {Qt::DisplayRole, Qn::ResourceFlagsRole, Qn::ResourceIconKeyRole}
    };
}

ItemOrder locaResourcesOrder()
{
    const auto groupIndex =
        [](const Qn::ResourceFlags& flags)
        {
            if (flags.testFlag(Qn::local_image))
                return 2;

            if (flags.testFlag(Qn::local_video))
                return 1;

            return 0;
        };

    return {
        [mainOrder = numericOrder(), groupIndex](const AbstractItem* lhs,
            const AbstractItem* rhs)
        {
            const auto lhsFlags = lhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
            const auto rhsFlags = rhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();

            const auto lhsGroupIndex = groupIndex(lhsFlags);
            const auto rhsGroupIndex = groupIndex(rhsFlags);

            if (lhsGroupIndex == rhsGroupIndex)
                return mainOrder.comp(lhs, rhs);

            return lhsGroupIndex < rhsGroupIndex;

        },
        {Qt::DisplayRole, Qn::ResourceFlagsRole}
    };
}

ItemOrder serverResourcesOrder()
{
    static const auto groupIndex =
        [](const Qn::ResourceFlags& flags)
        {
            return flags.testFlag(Qn::web_page) ? 1 : 0;
        };

    return {
        [mainOrder = numericOrder()](const AbstractItem* lhs,
            const AbstractItem* rhs)
        {
            const auto lhsFlags = lhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();
            const auto rhsFlags = rhs->data(Qn::ResourceFlagsRole).value<Qn::ResourceFlags>();

            const auto lhsGroupIndex = groupIndex(lhsFlags);
            const auto rhsGroupIndex = groupIndex(rhsFlags);

            if (lhsGroupIndex == rhsGroupIndex)
                return mainOrder.comp(lhs, rhs);

            return lhsGroupIndex < rhsGroupIndex;

        },
        {Qt::DisplayRole, Qn::ResourceFlagsRole}
    };
}

} // namespace entity_resource_tree
} // namespace nx::vms::client::desktop
