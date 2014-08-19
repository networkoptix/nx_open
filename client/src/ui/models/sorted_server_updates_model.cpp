#include "sorted_server_updates_model.h"

QnSortedServerUpdatesModel::QnSortedServerUpdatesModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

bool QnSortedServerUpdatesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QnServerUpdatesModel::Item *litem = reinterpret_cast<QnServerUpdatesModel::Item*>(left.internalPointer());
    QnServerUpdatesModel::Item *ritem = reinterpret_cast<QnServerUpdatesModel::Item*>(right.internalPointer());

    bool lonline = litem->server()->getStatus() == Qn::Online;
    bool ronline = ritem->server()->getStatus() == Qn::Online;

    if (lonline != ronline)
        return lonline;

    QString lname = left.data(Qt::DisplayRole).toString();
    QString rname = right.data(Qt::DisplayRole).toString();

    return lname < rname;
}
