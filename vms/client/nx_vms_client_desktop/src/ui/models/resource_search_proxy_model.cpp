// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_search_proxy_model.h"

#include <client/client_globals.h>
#include <core/resource/resource.h>

#include <nx/utils/pending_operation.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/resource/search_helper.h>

using namespace nx::vms::client::desktop;

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject* parent):
    base_type(parent),
    m_updateParentsOp(new nx::utils::PendingOperation())
{
    setRecalculateFilterAfterLongMove(false);

    m_updateParentsOp->setIntervalMs(1);
    m_updateParentsOp->setFlags(nx::utils::PendingOperation::FireOnlyWhenIdle);
    m_updateParentsOp->setCallback(
        [this]()
        {
            const auto indicesToUpdate = m_parentIndicesToUpdate;
            m_parentIndicesToUpdate.clear();

            for (const auto& index: indicesToUpdate)
            {
                if (index.isValid())
                    emit sourceModel()->dataChanged(index, index);
            }
        });
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel()
{
}

void QnResourceSearchProxyModel::setSourceModel(QAbstractItemModel* value)
{
    if (value == sourceModel())
        return;

    if (sourceModel())
        sourceModel()->disconnect(this);

    m_parentIndicesToUpdate.clear();

    base_type::setSourceModel(value);

    if (!sourceModel())
        return;

    // Update parent filtered state after child filtered state was possibly changed.
    connect(sourceModel(), &QAbstractItemModel::dataChanged, this,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight)
        {
            const auto parent = topLeft.parent();
            NX_ASSERT(parent == bottomRight.parent());

            if (!parent.isValid())
                return;

            m_parentIndicesToUpdate.push_back(parent);
            m_updateParentsOp->requestOperation();
        });
}

QnResourceSearchQuery QnResourceSearchProxyModel::query() const
{
    return m_query;
}

QModelIndex QnResourceSearchProxyModel::setQuery(const QnResourceSearchQuery& query)
{
    // There is no query equality check since current root node may be different for the same
    // query. E.g. for local resources in connected and disconnected states.
    m_query = query;
    invalidateFilter();

    m_currentRootNode =
        [this]()
        {
            if (m_query.allowedNode == QnResourceSearchQuery::kAllowAllNodeTypes)
                return QModelIndex();

            for (int row = 0; row < rowCount(); ++row)
            {
                const auto rowIndex = index(row, Qn::NameColumn);
                const auto nodeType =
                    rowIndex.data(Qn::NodeTypeRole).value<ResourceTree::NodeType>();
                if (nodeType == m_query.allowedNode)
                    return rowIndex;
            }
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
    invalidateFilter();
}

bool QnResourceSearchProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    const bool isSearchMode = !m_query.text.isEmpty()
        || m_query.allowedNode != QnResourceSearchQuery::kAllowAllNodeTypes;

    if (!isSearchMode)
        return true;

    QModelIndex index = sourceModel()->index(sourceRow, Qn::NameColumn, sourceParent);
    if (!index.isValid())
        return true;

    const auto allowedNode = m_query.allowedNode;
    if (!isAcceptedIndex(index, allowedNode))
        return false;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();

    // Filter out all nodes except allowed one if set
    static const auto searchGroupNodes = QSet<NodeType>({
        NodeType::servers,
        NodeType::layouts,
        NodeType::showreels,
        NodeType::videoWalls,
        NodeType::integrations,
        NodeType::webPages,
        NodeType::analyticsEngines,
        NodeType::users,
        NodeType::localResources});

    if (allowedNode != QnResourceSearchQuery::kAllowAllNodeTypes
        && allowedNode != nodeType
        && searchGroupNodes.contains(nodeType))
    {
        return false;
    }

    const int childCount = sourceModel()->rowCount(index);
    for (int i = 0; i < childCount; i++)
    {
        if (filterAcceptsRow(i, index))
            return true;
    }

    if (auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>())
        return isResourceMatchesQuery(resource, m_query);
    else if (!searchGroupNodes.contains(nodeType))
        return isRepresentationMatchesQuery(index, m_query);

    return false;
}

bool QnResourceSearchProxyModel::isAcceptedIndex(
    QModelIndex index,
    NodeType allowedNode) const
{
    const auto nodeType = index.data(Qn::NodeTypeRole).value<NodeType>();
    if (isRejectedNodeType(nodeType, allowedNode))
        return false;

    const auto resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if (!resource)
        return true;

    const auto resourceFlags = resource->flags();
    switch (allowedNode)
    {
        case NodeType::servers:
            return resourceFlags.testFlag(Qn::server);
        case NodeType::layouts:
            return resourceFlags.testFlag(Qn::layout)
                && (!resourceFlags.testFlag(Qn::local)
                    || resourceFlags.testFlag(Qn::local_intercom_layout));
        case NodeType::showreels:
            return true;
        case NodeType::videoWalls:
            return resourceFlags.testFlag(Qn::videowall);
        case NodeType::webPages:
            return resourceFlags.testFlag(Qn::web_page);
        case NodeType::users:
            return resourceFlags.testFlag(Qn::user);
        case NodeType::localResources:
            return true;
        default:
            return true;
    }
}

bool QnResourceSearchProxyModel::isRejectedNodeType(
    NodeType nodeType, NodeType allowedNodeType) const
{
    switch (nodeType)
    {
        case NodeType::separator:
        case NodeType::currentSystem:
        case NodeType::currentUser:
        case NodeType::allCamerasAccess:
        case NodeType::allLayoutsAccess:
        case NodeType::sharedLayouts:
        case NodeType::sharedResources:
            return true;
        case NodeType::videoWallItem:
        case NodeType::videoWallMatrix:
            return allowedNodeType == NodeType::videoWalls; //< Show videowall without children.
        case NodeType::recorder:
            return allowedNodeType == NodeType::servers; //< Filter non-resource server children.
        case NodeType::resource:
            return allowedNodeType == NodeType::showreels;
        default:
            return false;
    }
}

bool QnResourceSearchProxyModel::isResourceMatchesQuery(QnResourcePtr resource,
    const QnResourceSearchQuery& query) const
{
    // Simply filter by text first.
    // Show everything that's allowed if nothing entered into search query text input.
    if (resources::search_helper::isSearchStringValid(m_query.text)
        && !resources::search_helper::matches(m_query.text, resource))
    {
        return false;
    }

    // Check if no further filtering is required.
    if (m_query.flags == 0)
        return true;

    // Show only resources with given flags.
    return resource->hasFlags(m_query.flags);
}

bool QnResourceSearchProxyModel::isRepresentationMatchesQuery(const QModelIndex& index,
    const QnResourceSearchQuery& query) const
{
    return index.data(Qt::DisplayRole).toString().contains(m_query.text, Qt::CaseInsensitive);
}
