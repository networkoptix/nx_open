// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_compare_helper.h"

#include <QtCore/QModelIndex>
#include <QtCore/QCollator>

#include <client/client_globals.h>
#include <core/resource/layout_resource.h>
#include <nx/vms/client/core/client_core_globals.h>
#include <nx/vms/client/desktop/resource/layout_resource.h>

namespace {

int localFlagsOrder(Qn::ResourceFlags flags)
{
    if (flags.testFlag(Qn::local_image))
        return 2;

    if (flags.testFlag(Qn::local_video))
        return 1;

    /* Exported layouts. */
    return 0;
};

QnResourcePtr getResource(const QModelIndex& index)
{
    return index.data(nx::vms::client::core::ResourceRole).value<QnResourcePtr>();
}

} // namespace

bool QnResourceCompareHelper::resourceLessThan(
    const QModelIndex& left,
    const QModelIndex& right,
    const QCollator& collator)
{
    QnResourcePtr l = getResource(left);
    QnResourcePtr r = getResource(right);

    if (l && r)
    {
        /* Local resources should be ordered by type first */
        if (l->hasFlags(Qn::local) && r->hasFlags(Qn::local))
        {
            int leftFlagsOrder = localFlagsOrder(l->flags());
            int rightFlagsOrder = localFlagsOrder(r->flags());
            if (leftFlagsOrder != rightFlagsOrder)
                return leftFlagsOrder < rightFlagsOrder;
        }

        if (l->hasFlags(Qn::layout) && r->hasFlags(Qn::layout))
        {
            auto leftLayout = l.dynamicCast<nx::vms::client::desktop::LayoutResource>();
            auto rightLayout = r.dynamicCast<nx::vms::client::desktop::LayoutResource>();
            NX_ASSERT(leftLayout && rightLayout);
            if (leftLayout && rightLayout)
            {
                if (leftLayout->isCrossSystem() != rightLayout->isCrossSystem())
                    return rightLayout->isCrossSystem();
                if (leftLayout->isShared() != rightLayout->isShared())
                    return leftLayout->isShared();
            }
        }

        // Web pages in plain list must be the last
        if (l->hasFlags(Qn::web_page) != r->hasFlags(Qn::web_page))
            return r->hasFlags(Qn::web_page);

        // Servers should go below the cameras
        if (l->hasFlags(Qn::server) != r->hasFlags(Qn::server))
            return r->hasFlags(Qn::server);

        bool leftIncompatible = (l->resourcePool() && l->getStatus() == nx::vms::api::ResourceStatus::incompatible);
        bool rightIncompatible = (r->resourcePool() && r->getStatus() == nx::vms::api::ResourceStatus::incompatible);
        if (leftIncompatible != rightIncompatible) /* One of the nodes is incompatible server node, but not both. */
            return rightIncompatible;
    }

    /* Sort by name. */
    QString leftDisplay = left.data(Qt::DisplayRole).toString();
    QString rightDisplay = right.data(Qt::DisplayRole).toString();
    int result = collator.compare(leftDisplay, rightDisplay);
    if (result != 0)
        return result < 0;

    /* We want the order to be defined even for items with the same name. */
    if (l && r)
        return l->getId() < r->getId();

    return l < r;
}
