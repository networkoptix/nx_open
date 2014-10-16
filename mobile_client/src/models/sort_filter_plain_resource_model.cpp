#include "sort_filter_plain_resource_model.h"

#include "mobile_client/mobile_client_roles.h"
#include "models/plain_resource_model.h"
#include "models/plain_resource_model_node.h"
#include "utils/common/string.h"

QnSortFilterPlainResourceModel::QnSortFilterPlainResourceModel(QObject *parent) :
    QSortFilterProxyModel(parent)
{
    QnPlainResourceModel *model = new QnPlainResourceModel(this);
    setSourceModel(model);
    setDynamicSortFilter(true);
    sort(0);
}

bool QnSortFilterPlainResourceModel::lessThan(const QModelIndex &left, const QModelIndex &right) const {
    int leftType = left.data(Qn::NodeTypeRole).toInt();
    int rightType = right.data(Qn::NodeTypeRole).toInt();

    QVariant leftServer = left.data(Qn::ServerResourceRole);
    QVariant rightServer = right.data(Qn::ServerResourceRole);

    if (leftServer != rightServer) {
        QString leftName = left.data(Qn::ServerNameRole).toString();
        QString rightName = right.data(Qn::ServerNameRole).toString();

        qDebug() << naturalStringLessThan(leftName, rightName);
        return naturalStringLessThan(leftName, rightName);
    }

    if (leftType != rightType) {
        qDebug() << (leftType == QnPlainResourceModel::ServerNode);
        return leftType == QnPlainResourceModel::ServerNode;
    }

    QString leftName = left.data(Qt::DisplayRole).toString();
    QString rightName = right.data(Qt::DisplayRole).toString();

    return naturalStringLessThan(leftName, rightName);
}
