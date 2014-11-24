#include "sorted_server_updates_model.h"

#include <core/resource/media_server_resource.h>

#include <utils/common/string.h>

QnSortedServerUpdatesModel::QnSortedServerUpdatesModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
}

bool QnSortedServerUpdatesModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    QnMediaServerResourcePtr lServer = left.data(Qn::MediaServerResourceRole).value<QnMediaServerResourcePtr>();
    QnMediaServerResourcePtr rServer = right.data(Qn::MediaServerResourceRole).value<QnMediaServerResourcePtr>();

    /* Security check. */
    if (!lServer || !rServer)
        return lServer < rServer;

    bool lonline = lServer->getStatus() == Qn::Online;
    bool ronline = rServer->getStatus() == Qn::Online;

    if (lonline != ronline)
        return lonline;

    QString lname = left.data(Qt::DisplayRole).toString();
    QString rname = right.data(Qt::DisplayRole).toString();

    int result = naturalStringCompare(lname, rname, Qt::CaseInsensitive, false);
    if(result != 0)
        return result < 0;

    /* We want the order to be defined even for items with the same name. */
    return lServer->getUniqueId() < rServer->getUniqueId();
}
