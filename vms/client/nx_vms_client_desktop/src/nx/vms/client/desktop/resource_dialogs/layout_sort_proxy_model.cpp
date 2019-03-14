#include "layout_sort_proxy_model.h"

#include <core/resource/resource.h>
#include <nx/vms/client/desktop/node_view/resource_node_view/resource_view_node_helpers.h>

namespace nx::vms::client::desktop {

using namespace node_view;

LayoutSortProxyModel::LayoutSortProxyModel(const QnUuid& currentUserId, QObject* parent):
    base_type(parent),
    m_currentUserId(currentUserId)
{
}

bool LayoutSortProxyModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto leftResource = getResource(sourceLeft);
    const auto rightResource = getResource(sourceRight);
    if (!leftResource || !rightResource)
        return !nextLessThan(sourceLeft, sourceRight);

    const bool leftIsShared = leftResource->getParentId().isNull();
    const bool rightIsShared = rightResource->getParentId().isNull();
    if (leftIsShared != rightIsShared)
        return rightIsShared;

    const auto source = sourceModel();
    const bool isUserItem =
        source->rowCount(sourceLeft) > 0 || source->rowCount(sourceRight) > 0;
    if (isUserItem)
    {
        if (leftResource && leftResource->getId() == m_currentUserId)
            return false;

        if (rightResource && rightResource->getId() == m_currentUserId)
            return true;
    }

    return !nextLessThan(sourceLeft, sourceRight);
}

} // namespace nx::vms::client::desktop
