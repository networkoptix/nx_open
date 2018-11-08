#include "resource_compare_helper.h"

#include <QtCore/QModelIndex>

#include <client/client_globals.h>

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/utils/string.h>

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

}

QnResourcePtr QnResourceCompareHelper::resource(const QModelIndex& index)
{
    return index.data(Qn::ResourceRole).value<QnResourcePtr>();
}

bool QnResourceCompareHelper::resourceLessThan(const QModelIndex& left, const QModelIndex& right)
{
    QnResourcePtr l = resource(left);
    QnResourcePtr r = resource(right);

    if (l && r)
    {
        bool leftIncompatible = (l->resourcePool() && l->getStatus() == Qn::Incompatible);
        bool rightIncompatible = (r->resourcePool() && r->getStatus() == Qn::Incompatible);
        if (leftIncompatible != rightIncompatible) /* One of the nodes is incompatible server node, but not both. */
            return rightIncompatible;

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
            auto leftLayout = l.dynamicCast<QnLayoutResource>();
            auto rightLayout = r.dynamicCast<QnLayoutResource>();
            NX_ASSERT(leftLayout && rightLayout);
            if (leftLayout && rightLayout
                && leftLayout->isShared() != rightLayout->isShared())
                return leftLayout->isShared();
        }

        // Web pages in plain list must be the last
        if (l->hasFlags(Qn::web_page) != r->hasFlags(Qn::web_page))
            return r->hasFlags(Qn::web_page);

        // Servers should go below the cameras
        if (l->hasFlags(Qn::server) != r->hasFlags(Qn::server))
            return r->hasFlags(Qn::server);
    }

    /* Sort by name. */
    QString leftDisplay = left.data(Qt::DisplayRole).toString();
    QString rightDisplay = right.data(Qt::DisplayRole).toString();
    int result = nx::utils::naturalStringCompare(leftDisplay, rightDisplay, Qt::CaseInsensitive);
    if (result != 0)
        return result < 0;

    /* We want the order to be defined even for items with the same name. */
    if (l && r)
        return l->getUniqueId() < r->getUniqueId();

    return l < r;
}
