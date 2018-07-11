#include "accessible_layout_sort_model.h"

#include <core/resource/user_resource.h>
#include <core/resource/layout_resource.h>

#include <common/common_module.h>
#include <client_core/client_core_module.h>
#include <nx/client/core/watchers/user_watcher.h>

#include <nx/client/desktop/resource_views/node_view/node_view_model.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node.h>
#include <nx/client/desktop/resource_views/node_view/nodes/view_node_helpers.h>

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

QModelIndex AccessibleLayoutSortModel::mapToSource(const QModelIndex &proxyIndex) const
{
    const auto res = base_type::mapToSource(proxyIndex);
    qWarning() << "+++ mapToSource" << proxyIndex << " to " << res;
    return res;
}

QModelIndex AccessibleLayoutSortModel::mapFromSource(const QModelIndex &sourceIndex) const
{
    const auto res = base_type::mapFromSource(sourceIndex);
    qWarning() << "+++ mapFromSource" << sourceIndex << " to " << res;
    return res;
}

bool AccessibleLayoutSortModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    qWarning() << "-------" << source_row << source_parent;
    return base_type::filterAcceptsRow(source_row, source_parent);
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

} // namespace desktop
} // namespace client
} // namespace nx

