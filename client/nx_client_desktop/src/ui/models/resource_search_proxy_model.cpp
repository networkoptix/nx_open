#include "resource_search_proxy_model.h"

#include <client/client_globals.h>

#include <core/resource/resource.h>

#include <nx/utils/string.h>

#include <utils/common/delayed.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject* parent):
    base_type(parent)
{
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel()
{
}

QnResourceSearchQuery QnResourceSearchProxyModel::query() const
{
    return m_query;
}

void QnResourceSearchProxyModel::setQuery(const QnResourceSearchQuery& query)
{
    if (m_query == query)
        return;

    m_query = query;

    setFilterWildcard(L'*' + query.text + L'*');
    invalidateFilterLater();
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
    executeDelayedParented([this]{ invalidateFilter(); }, kDefaultDelay, this);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(
    int sourceRow,
    const QModelIndex& sourceParent) const
{
    if (m_query.isEmpty())
        return m_defaultBehavior == DefaultBehavior::showAll;

    QModelIndex root = (sourceParent.column() > Qn::NameColumn)
        ? sourceParent.sibling(sourceParent.row(), Qn::NameColumn)
        : sourceParent;

    QModelIndex index = sourceModel()->index(sourceRow, 0, root);
    if (!index.isValid())
        return true;

    const auto nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    switch (nodeType)
    {
        case Qn::CurrentSystemNode:
        case Qn::CurrentUserNode:
        case Qn::SeparatorNode:
        case Qn::LocalSeparatorNode:
        case Qn::BastardNode:
        case Qn::AllCamerasAccessNode:
        case Qn::AllLayoutsAccessNode:
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
        return false;
    }

    // Simply filter by text first.
    if (!base_type::filterAcceptsRow(sourceRow, root))
        return false;

    // Check if no further filtering is required.
    if (m_query.flags == 0)
        return true;

    // Show only resources with given flags.
    QnResourcePtr resource = this->resource(index);
    return resource && resource->hasFlags(m_query.flags);
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
