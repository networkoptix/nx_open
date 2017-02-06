#include "resource_search_proxy_model.h"

#include <client/client_globals.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource/resource.h>

#include <nx/network/nettools.h>

#include <nx/utils/string.h>
#include <utils/common/delete_later.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent):
    QSortFilterProxyModel(parent),
    m_invalidating(false)
{
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel()
{
}

void QnResourceSearchProxyModel::addCriterion(const QnResourceCriterion &criterion)
{
    m_criterionGroup.addCriterion(criterion);

    invalidateFilterLater();
}

bool QnResourceSearchProxyModel::removeCriterion(const QnResourceCriterion &criterion)
{
    bool removed = m_criterionGroup.removeCriterion(criterion);
    if (removed)
        invalidateFilterLater();

    return removed;
}

void QnResourceSearchProxyModel::clearCriteria()
{
    if (m_criterionGroup.empty())
        return;

    m_criterionGroup.clear();
    invalidateFilterLater();
}

const QnResourceCriterionGroup & QnResourceSearchProxyModel::criteria()
{
    return m_criterionGroup;
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

    clearCriteria();
    setFilterWildcard(L'*' + query.text + L'*');
    if (query.text.isEmpty())
    {
        addCriterion(QnResourceCriterionGroup(QnResourceCriterion::Reject,
            QnResourceCriterion::Reject));
    }
    else
    {
        addCriterion(QnResourceCriterionGroup(query.text));
        addCriterion(QnResourceCriterion(Qn::user));
        addCriterion(QnResourceCriterion(Qn::layout));
    }
    if (query.flags != 0)
    {
        addCriterion(QnResourceCriterion(query.flags, QnResourceProperty::flags,
            QnResourceCriterion::Next, QnResourceCriterion::Reject));
    }
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
    QMetaObject::invokeMethod(this, "invalidateFilter", Qt::QueuedConnection);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if (!index.isValid())
        return true;

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();

    if (isSeparatorNode(nodeType))
        return false;

    if (nodeType == Qn::UsersNode)
        return false; /* We don't want users in search. */

    if (nodeType == Qn::RecorderNode
        || nodeType == Qn::LocalResourcesNode
        || nodeType == Qn::OtherSystemsNode)
    {
        for (int i = 0; i < sourceModel()->rowCount(index); i++)
        {
            if (filterAcceptsRow(i, index))
                return true;
        }
        return false;
    }

    /* Simply filter by text. */
    if (nodeType == Qn::CloudSystemNode)
        return base_type::filterAcceptsRow(source_row, source_parent);

    QnResourcePtr resource = this->resource(index);
    if (!resource)
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup.check(resource);
    if (operation != QnResourceCriterion::Accept)
        return false;

    return true;
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
