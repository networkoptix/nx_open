#include "qml_sort_filter_proxy_model.h"

QnQmlSortFilterProxyModel::QnQmlSortFilterProxyModel()
    :
    base_type()
{
}


QnQmlSortFilterProxyModel::QnQmlSortFilterProxyModel(QObject* parent)
    :
    base_type(parent)
{
}

QAbstractItemModel* QnQmlSortFilterProxyModel::model() const
{
    return sourceModel();
}

void QnQmlSortFilterProxyModel::setModel(QAbstractItemModel* targetModel)
{
    if (targetModel == model())
        return;

    setSourceModel(targetModel);
    emit modelChanged();
}


