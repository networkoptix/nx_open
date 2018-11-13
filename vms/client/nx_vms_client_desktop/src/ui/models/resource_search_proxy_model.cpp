#include "resource_search_proxy_model.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <nx/utils/string.h>

#include <utils/common/delayed.h>

#include <ui/models/resource/resource_tree_model.h>
#include <ui/workbench/workbench_context.h>

#include <nx/vms/client/desktop/resource_views/data/node_type.h>
#include <nx/vms/client/desktop/resources/search_helper.h>

using namespace nx::vms::client::desktop;

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject* parent):
    base_type(parent)
{
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel() = default;

QnResourceSearchQuery QnResourceSearchProxyModel::query() const
{
    return m_query;
}

QModelIndex QnResourceSearchProxyModel::setQuery(const QnResourceSearchQuery& query)
{
    m_query = query;

    setFilterWildcard(L'*' + query.text + L'*');
    invalidateFilterLater();

    QnResourceTreeModel* resourceTreeModel = qobject_cast<QnResourceTreeModel*>(sourceModel());
    if (!resourceTreeModel)
        return QModelIndex();

    bool isLoggedIn = !resourceTreeModel->context()->user().isNull();
      
    m_currentRootNode =
        [this, isLoggedIn]()
        {
            if (m_query.allowedNode == QnResourceSearchQuery::kAllowAllNodeTypes)
                return QModelIndex();

            if (!isLoggedIn && m_query.allowedNode == ResourceTreeNodeType::localResources)
                return QModelIndex();

            const int count = rowCount();
            if (!count)
                return QModelIndex();

            if (count == 1)
                return index(0, 0);

            NX_ASSERT(false, "There should not be more than one root node.");
            return QModelIndex();
        }();

    return m_currentRootNode;
}

QnResourceSearchProxyModel::DefaultBehavior QnResourceSearchProxyModel::defaultBehavor() const
{
    return m_defaultBehavior;
}

void QnResourceSearchProxyModel::setDefaultBehavior(DefaultBehavior value)
{
    if (m_defaultBehavior == value)
        return;

    m_defaultBehavior = value;
    invalidateFilterLater();
}

void QnResourceSearchProxyModel::invalidateFilter()
{
    m_invalidating = false;
    QSortFilterProxyModel::invalidateFilter();
    emit criteriaChanged();
}

void QnResourceSearchProxyModel::invalidateFilterLater()
{
    if (m_invalidating)
        return; /* Already waiting for invalidation. */

    m_invalidating = true;
    executeDelayedParented([this]{ invalidateFilter(); }, this);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    const bool searchMode = !m_query.text.isEmpty()
        || m_query.allowedNode != QnResourceSearchQuery::kAllowAllNodeTypes;

    if (!searchMode && m_defaultBehavior != DefaultBehavior::showAll)
        return false;

    const bool showAllMode = m_defaultBehavior == DefaultBehavior::showAll;

    QModelIndex root = (sourceParent.column() > Qn::NameColumn)
        ? sourceParent.sibling(sourceParent.row(), Qn::NameColumn)
        : sourceParent;

    QModelIndex index = sourceModel()->index(sourceRow, 0, root);
    if (!index.isValid())
        return true;

    using NodeType = ResourceTreeNodeType;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

    // Handles visibility of nodes in search mode
    switch(nodeType)
    {
        case NodeType::servers:
        case NodeType::userResources:
        case NodeType::layouts:
        case NodeType::users:
        case NodeType::otherSystems:
            if (searchMode)
                return false;
            break;

        case NodeType::filteredServers:
        case NodeType::filteredCameras:
        case NodeType::filteredLayouts:
        case NodeType::filteredUsers:
        case NodeType::filteredVideowalls:
            if (!searchMode)
                return false;
            break;
        default:
            break;
    }

    if (searchMode)
    {
        const auto allowedNode = m_query.allowedNode;

        static const auto searchGroupNodes = QSet<NodeType>({
            NodeType::filteredServers,
            NodeType::filteredCameras,
            NodeType::filteredLayouts,
            NodeType::layoutTours,
            NodeType::filteredVideowalls,
            NodeType::webPages,
            NodeType::analyticsEngines,
            NodeType::filteredUsers,
            NodeType::localResources});

        if (allowedNode != QnResourceSearchQuery::kAllowAllNodeTypes
            && allowedNode != nodeType
            && searchGroupNodes.contains(nodeType))
        {
            return false; // Filter out all nodes except allowed one
        }

        // We don't show servers and videowalls in case of search.
        const auto resource = this->resource(index);
        const auto scope = index.data(Qn::ResourceTreeScopeRole).value<QnResourceTreeModel::Scope>();
        if (resource && scope == QnResourceTreeModel::FullScope)
        {
            const auto parentNodeType = sourceParent.data(Qn::NodeTypeRole).value<NodeType>();
            if (parentNodeType != NodeType::filteredServers && resource->hasFlags(Qn::server))
                return false;

            if (parentNodeType != NodeType::filteredVideowalls && resource->hasFlags(Qn::videowall))
                return false;
        }
    }
    else if (showAllMode)
        return true;

    switch (nodeType)
    {
        case NodeType::currentSystem:
        case NodeType::currentUser:
        case NodeType::separator:
        case NodeType::localSeparator:
        case NodeType::bastard:
        case NodeType::allCamerasAccess:
        case NodeType::allLayoutsAccess:
            return false;
        default:
            break;
    }

    const int childCount = sourceModel()->rowCount(index);
    const bool hasChildren = childCount > 0;
    if (hasChildren)
    {
        for (int i = 0; i < childCount; i++)
        {
            if (filterAcceptsRow(i, index))
                return true;
        }
    }

    const auto resource = QnResourceSearchProxyModel::resource(index);
    if (!resource)
        return false;

    // Simply filter by text first.
    // Show everything that's allowed if nothing entered into search query text input.
    if (resources::search_helper::isSearchStringValid(m_query.text) 
        && !resources::search_helper::matches(m_query.text, resource))
        return false;

    // Check if no further filtering is required.
    if (m_query.flags == 0)
        return true;

    // Show only resources with given flags.
    return resource->hasFlags(m_query.flags);
}


bool QnResourceSearchProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QVariant leftData = sourceModel()->data(left);
    QVariant rightData = sourceModel()->data(right);
    if (leftData.type() == QVariant::String && rightData.type() == QVariant::String)
    {
        QString ls = leftData.toString();
        QString rs = rightData.toString();
        return nx::utils::naturalStringLess(ls, rs);
    }
    else
    {
        // Throw the rest situation to base class to handle
        return QSortFilterProxyModel::lessThan(left, right);
    }
}
