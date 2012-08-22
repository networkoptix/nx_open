#include "resource_search_proxy_model.h"
#include "resource_pool_model.h"

#include <utils/common/delete_later.h>
#include <core/resourcemanagment/resource_criterion.h>
#include <core/resource/resource.h>

#include <ui/workbench/workbench_globals.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent): 
    QSortFilterProxyModel(parent),
    m_invalidating(false)
{}

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

    Qn::NodeType nodeType = static_cast<Qn::NodeType>(index.data(Qn::NodeTypeRole).value<int>());
    if(nodeType == Qn::UsersNode)
        return false; /* We don't want users in search. */

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if(resource.isNull())
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup.check(resource);
    if(operation != QnResourceCriterion::Accept)
        return false;

    return true;
}

