// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_intermediate_model.h"

#include <core/resource/resource.h>
#include <nx/utils/math/math.h>

namespace nx::vms::client::desktop::joystick {

LayoutIntermidiateModel::LayoutIntermidiateModel(QObject* parent): base_type(parent)
{
}

LayoutIntermidiateModel::~LayoutIntermidiateModel()
{
}

QHash<int, QByteArray> LayoutIntermidiateModel::roleNames() const
{
    QHash<int, QByteArray> roles = base_type::roleNames();
    roles[LogicalIdRole] = "logicalId";
    return roles;
}

QVariant LayoutIntermidiateModel::data(const QModelIndex& index, int role) const
{
    const QModelIndex sourceIndex = mapToSource(index);

    switch (role)
    {
        case LogicalIdRole:
        {
             const auto resourcePtr = qvariant_cast<QnResourcePtr>(
                 sourceModel()->data(sourceIndex, core::ResourceRole));
             return resourcePtr->logicalId();
        }
        case Qn::ItemUuidRole:
        {
             const auto resourcePtr = qvariant_cast<QnResourcePtr>(
                 sourceModel()->data(sourceIndex, core::ResourceRole));
             return resourcePtr->getId().toString();
        }
        default:
            return sourceModel()->data(sourceIndex, role);
    }
}

} // namespace nx::vms::client::desktop::joystick
