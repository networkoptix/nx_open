#include "sorted_server_updates_model.h"

QnSortedServerUpdatesModel::QnSortedServerUpdatesModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

void QnSortedServerUpdatesModel::setFilter(const QSet<QnId> &filter) {
    m_filter = filter;
    invalidateFilter();
}

bool QnSortedServerUpdatesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QnServerUpdatesModel::Item *litem = reinterpret_cast<QnServerUpdatesModel::Item*>(left.internalPointer());
    QnServerUpdatesModel::Item *ritem = reinterpret_cast<QnServerUpdatesModel::Item*>(right.internalPointer());

    bool lonline = litem->server()->getStatus() == QnResource::Online;
    bool ronline = ritem->server()->getStatus() == QnResource::Online;

    if (lonline != ronline)
        return lonline;

    QString lname = left.data(Qt::DisplayRole).toString();
    QString rname = right.data(Qt::DisplayRole).toString();

    return lname < rname;
}

bool QnSortedServerUpdatesModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    Q_UNUSED(sourceRow)

    QnServerUpdatesModel::Item *item = reinterpret_cast<QnServerUpdatesModel::Item*>(sourceModel()->index(sourceRow, 0, sourceParent).internalPointer());
    return m_filter.isEmpty() || m_filter.contains(item->server()->getId());
}
