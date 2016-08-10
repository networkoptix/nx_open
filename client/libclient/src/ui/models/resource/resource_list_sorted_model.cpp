#include "resource_list_sorted_model.h"

QnResourceListSortedModel::QnResourceListSortedModel(QObject *parent):
    base_type(parent)
{
    setDynamicSortFilter(true);
    setSortRole(Qt::DisplayRole);
    setSortCaseSensitivity(Qt::CaseInsensitive);
}

QnResourceListSortedModel::~QnResourceListSortedModel()
{
}

bool QnResourceListSortedModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    return resourceLessThan(left, right);
}
