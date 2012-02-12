#include "resource_search_proxy_model.h"
#include "resource_model.h"
#include <core/resourcemanagment/resource_criterion.h>
#include <core/resource/resource.h>
#include <utils/common/delete_later.h>

QnResourceSearchProxyModel::QnResourceSearchProxyModel(QObject *parent): 
    QSortFilterProxyModel(parent),
    m_invalidateListener(NULL),
    m_criterionGroup(new QnResourceCriterionGroup())
{}

QnResourceSearchProxyModel::~QnResourceSearchProxyModel() {
    return;
}

void QnResourceSearchProxyModel::addCriterion(QnResourceCriterion *criterion) {
    m_criterionGroup->addCriterion(criterion);

    invalidateFilterLater();
}

bool QnResourceSearchProxyModel::removeCriterion(QnResourceCriterion *criterion) {
    bool removed = m_criterionGroup->removeCriterion(criterion);
    if(removed)
        invalidateFilterLater();

    return removed;
}

bool QnResourceSearchProxyModel::replaceCriterion(QnResourceCriterion *from, QnResourceCriterion *to) {
    bool replaced = m_criterionGroup->replaceCriterion(from, to);
    if(replaced)
        invalidateFilterLater();

    return replaced;
}

QnResourcePtr QnResourceSearchProxyModel::resource(const QModelIndex &index) const {
    return data(index, Qn::ResourceRole).value<QnResourcePtr>();
}

void QnResourceSearchProxyModel::invalidateFilter() {
    if(m_invalidateListener != NULL) {
        disconnect(m_invalidateListener, NULL, this, NULL);
        m_invalidateListener = NULL;
    }

    QSortFilterProxyModel::invalidateFilter();
    emit criteriaChanged();
}

void QnResourceSearchProxyModel::invalidateFilterLater() {
    if(m_invalidateListener != NULL)
        return; /* Already waiting for invalidation. */

    m_invalidateListener = new QObject(this);
    connect(m_invalidateListener, SIGNAL(destroyed()), this, SLOT(invalidateFilter()));
    qnDeleteLater(m_invalidateListener);
}

bool QnResourceSearchProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const {
    QModelIndex index = sourceModel()->index(source_row, 0, source_parent);
    if(!index.isValid())
        return true;

    QnResourcePtr resource = index.data(Qn::ResourceRole).value<QnResourcePtr>();
    if(resource.isNull())
        return true;

    QnResourceCriterion::Operation operation = m_criterionGroup->check(resource);
    if(operation != QnResourceCriterion::ACCEPT)
        return false;

    return true;
}

