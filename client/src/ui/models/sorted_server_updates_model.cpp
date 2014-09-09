#include "sorted_server_updates_model.h"

#include <core/resource/media_server_resource.h>

QnSortedServerUpdatesModel::QnSortedServerUpdatesModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

bool QnSortedServerUpdatesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {

    QnMediaServerResourcePtr lServer = left.data(Qn::MediaServerResourceRole).value<QnMediaServerResourcePtr>();
    QnMediaServerResourcePtr rServer = right.data(Qn::MediaServerResourceRole).value<QnMediaServerResourcePtr>();

    bool lonline = lServer->getStatus() == Qn::Online;
    bool ronline = rServer->getStatus() == Qn::Online;

    if (lonline != ronline)
        return lonline;

    QString lname = left.data(Qt::DisplayRole).toString();
    QString rname = right.data(Qt::DisplayRole).toString();

    return lname < rname;
}
