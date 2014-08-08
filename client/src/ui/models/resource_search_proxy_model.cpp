#include "resource_search_proxy_model.h"
#include "resource_pool_model.h"

#include <utils/common/delete_later.h>
#include <utils/network/nettools.h>
#include <utils/common/string.h>

#include <core/resource_management/resource_criterion.h>
#include <core/resource/resource.h>

#include <client/client_globals.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent): 
    QSortFilterProxyModel(parent),
    m_invalidating(false)
{
}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel() {
    return;
}

void QnResourceSearchProxyModel::addCriterion(const QnResourceCriterion &criterion) {
    m_criterionGroup.addCriterion(criterion);

    invalidateFilterLater();
}

bool QnResourceSearchProxyModel::removeCriterion(const QnResourceCriterion &criterion) {
    bool removed = m_criterionGroup.removeCriterion(criterion);
    if(removed)
        invalidateFilterLater();

    return removed;
}

void QnResourceSearchProxyModel::clearCriteria() {
    if(m_criterionGroup.empty())
        return;

    m_criterionGroup.clear();
    invalidateFilterLater();
}

QnResourcePtr QnResourceSearchProxyModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

void QnResourceSearchProxyModel::invalidateFilter() {
    m_invalidating = false;
    QSortFilterProxyModel::invalidateFilter();
    emit criteriaChanged();
}

void QnResourceSearchProxyModel::invalidateFilterLater() {
    if(m_invalidating)
        return; /* Already waiting for invalidation. */

    m_invalidating = true;
    QMetaObject::invokeMethod(this, "invalidateFilter", Qt::QueuedConnection);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if(!index.isValid())
        return true;

    Qn::NodeType nodeType = index.data(Qn::NodeTypeRole).value<Qn::NodeType>();
    if(nodeType == Qn::UsersNode)
        return false; /* We don't want users in search. */

    if(nodeType == Qn::RecorderNode) {
        for (int i = 0; i < sourceModel()->rowCount(index); i++) {
            if (filterAcceptsRow(i, index))
                return true;
        }
        return false;
    }

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if(resource.isNull())
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup.check(resource);
    if(operation != QnResourceCriterion::Accept)
        return false;

    return true;
}


bool QnResourceSearchProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QVariant leftData = sourceModel()->data(left);
    QVariant rightData = sourceModel()->data(right);
    if( leftData.type() == QVariant::String && rightData.type() == QVariant::String ) {
        QString ls = leftData.toString();
        QString rs = rightData.toString();
        return naturalStringCompare(ls,rs,Qt::CaseInsensitive,false) >0;
    } else {
        // Throw the rest situation to base class to handle 
        return QSortFilterProxyModel::lessThan(left,right);
    }
}
