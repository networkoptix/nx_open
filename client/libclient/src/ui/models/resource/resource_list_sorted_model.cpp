#include "resource_list_sorted_model.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>

#include <nx/utils/string.h>

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

QnResourcePtr QnResourceListSortedModel::resource(const QModelIndex &index) const
{
    return index.data(Qn::ResourceRole).value<QnResourcePtr>();
}

bool QnResourceListSortedModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QnResourcePtr l = resource(left);
    QnResourcePtr r = resource(right);

    if (!l || !r)
        return l < r;

    if (l->getTypeId() != r->getTypeId())
    {
        QnVirtualCameraResourcePtr lcam = l.dynamicCast<QnVirtualCameraResource>();
        QnVirtualCameraResourcePtr rcam = r.dynamicCast<QnVirtualCameraResource>();
        if (!lcam || !rcam)
            return lcam < rcam;
    }

    if (l->hasFlags(Qn::layout) && r->hasFlags(Qn::layout))
    {
        QnLayoutResourcePtr leftLayout = l.dynamicCast<QnLayoutResource>();
        QnLayoutResourcePtr rightLayout = r.dynamicCast<QnLayoutResource>();
        NX_ASSERT(leftLayout && rightLayout);
        if (!leftLayout || !rightLayout)
            return leftLayout < rightLayout;

        if (leftLayout->isShared() != rightLayout->isShared())
            return leftLayout->isShared();
    }

    {
        /* Sort by name. */
        QString leftDisplay = left.data(Qt::DisplayRole).toString();
        QString rightDisplay = right.data(Qt::DisplayRole).toString();
        int result = nx::utils::naturalStringCompare(leftDisplay, rightDisplay, Qt::CaseInsensitive);
        if (result != 0)
            return result < 0;
    }

    /* We want the order to be defined even for items with the same name. */
    return l->getUniqueId() < r->getUniqueId();
}
