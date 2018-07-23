#include "accessible_layout_sort_model.h"

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <nx/client/core/watchers/user_watcher.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>
#include <nx/client/desktop/resource_views/node_view/details/node_view_helpers.h>

QnUuid getCurrentUserId()
{
    const auto commonModule = qnClientCoreModule->commonModule();
    const auto userWatcher = commonModule->instance<nx::client::core::UserWatcher>();
    const auto currentUser = userWatcher->user();
    return currentUser->getId();
}

namespace nx {
namespace client {
namespace desktop {

AccessibleLayoutSortModel::AccessibleLayoutSortModel(QObject* parent)
{
}

void AccessibleLayoutSortModel::setFilter(const QString& filter)
{
    m_filter = filter;
    invalidateFilter();
}

bool AccessibleLayoutSortModel::lessThan(
    const QModelIndex& sourceLeft,
    const QModelIndex& sourceRight) const
{
    const auto left = NodeViewModel::nodeFromIndex(sourceLeft);
    const auto right = NodeViewModel::nodeFromIndex(sourceRight);
    if (!left || !right)
        return !base_type::lessThan(sourceLeft, sourceRight);

    const bool isUser = left->childrenCount() > 0 || right->childrenCount() > 0;
    const auto leftResource = helpers::getResource(left);
    const auto rightResource = helpers::getResource(right);
    if (!leftResource || !rightResource)
        return !base_type::lessThan(sourceLeft, sourceRight);

    if (isUser)
    {
        const auto currentUserId = getCurrentUserId();
        if (leftResource && leftResource->getId() == currentUserId)
            return false;

        if (rightResource && rightResource->getId() == currentUserId)
            return true;
    }

    return !base_type::lessThan(sourceLeft, sourceRight);
}

bool AccessibleLayoutSortModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex &sourceParent) const
{
    if (m_filter.isEmpty())
        return true;

    const auto source = sourceModel();
    if (!source)
        return true;

    const auto index = source->index(sourceRow, node_view::nameColumn, sourceParent);
    const auto text = index.data().toString();
    const bool containsFilter = text.contains(m_filter);
    if (containsFilter)
        return true;

    const auto childrenCount = source->rowCount(index);
    for (int i = 0; i != childrenCount; ++i)
    {
        if (filterAcceptsRow(i, index))
            return true;
    }
    return false;
}

} // namespace desktop
} // namespace client
} // namespace nx

