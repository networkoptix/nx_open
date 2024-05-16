// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <type_traits>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QAbstractListModel>
#include <QtCore/QAbstractTableModel>
#include <QtCore/QAbstractProxyModel>
#include <QtCore/QIdentityProxyModel>
#include <QtCore/QSortFilterProxyModel>

#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>

template<class BaseModel>
class ScopedModelOperations: public BaseModel
{
    static_assert(std::is_base_of<QAbstractItemModel, BaseModel>::value,
        "BaseModel must be derived from QAbstractItemModel");

public:
    using BaseModel::BaseModel;

    class ScopedReset: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        explicit ScopedReset(ScopedModelOperations* model, bool condition = true):
            base_type([model]() { model->endResetModel(); })
        {
            if (condition)
                model->beginResetModel();
            else
                disarm();
        }
    };

    class ScopedInsertColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedInsertColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last, bool condition = true)
            :
            base_type([model]() { model->endInsertColumns(); })
        {
            if (condition)
                model->beginInsertColumns(parent, first, last);
            else
                disarm();
        }

        ScopedInsertColumns(ScopedModelOperations* model, int first, int last, bool condition = true):
            ScopedInsertColumns(model, QModelIndex(), first, last, condition)
        {
        }
    };

    class ScopedInsertRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedInsertRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last, bool condition = true)
            :
            base_type([model]() { model->endInsertRows(); })
        {
            if (condition)
                model->beginInsertRows(parent, first, last);
            else
                disarm();
        }

        ScopedInsertRows(ScopedModelOperations* model, int first, int last, bool condition = true):
            ScopedInsertRows(model, QModelIndex(), first, last, condition)
        {
        }
    };

    class ScopedRemoveColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedRemoveColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last, bool condition = true)
            :
            base_type([model]() { model->endRemoveColumns(); })
        {
            if (condition)
                model->beginRemoveColumns(parent, first, last);
            else
                disarm();
        }

        ScopedRemoveColumns(ScopedModelOperations* model, int first, int last, bool condition = true):
            ScopedRemoveColumns(model, QModelIndex(), first, last, condition)
        {
        }
    };

    class ScopedRemoveRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedRemoveRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last, bool condition = true)
            :
            base_type([model]() { model->endRemoveRows(); })
        {
            if (condition)
                model->beginRemoveRows(parent, first, last);
            else
                disarm();
        }

        ScopedRemoveRows(ScopedModelOperations* model, int first, int last, bool condition = true):
            ScopedRemoveRows(model, QModelIndex(), first, last, condition)
        {
        }
    };

    class ScopedMoveColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedMoveColumns(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos, bool condition = true)
            :
            base_type([model]() { model->endMoveColumns(); })
        {
            if (!condition || !NX_ASSERT(model->beginMoveColumns(sourceParent, sourceFirst,
                sourceLast, destinationParent, destinationPos)))
            {
                disarm();
            }
        }

        ScopedMoveColumns(ScopedModelOperations* model, int sourceFirst, int sourceLast,
            int destinationPos, bool condition = true)
            :
            ScopedMoveColumns(model, QModelIndex(), sourceFirst, sourceLast,
                QModelIndex(), destinationPos, condition)
        {
        }
    };

    class ScopedMoveRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedMoveRows(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos, bool condition = true)
            :
            base_type([model]() { model->endMoveRows(); })
        {
            if (!condition || !NX_ASSERT(model->beginMoveRows(sourceParent, sourceFirst,
                sourceLast, destinationParent, destinationPos)))
            {
                disarm();
            }
        }

        ScopedMoveRows(ScopedModelOperations* model, int sourceFirst, int sourceLast,
            int destinationPos, bool condition = true)
            :
            ScopedMoveRows(model, QModelIndex(), sourceFirst, sourceLast,
                QModelIndex(), destinationPos, condition)
        {
        }
    };
};

template class NX_UTILS_API ScopedModelOperations<QAbstractItemModel>;
template class NX_UTILS_API ScopedModelOperations<QAbstractListModel>;
template class NX_UTILS_API ScopedModelOperations<QAbstractTableModel>;
template class NX_UTILS_API ScopedModelOperations<QAbstractProxyModel>;
template class NX_UTILS_API ScopedModelOperations<QIdentityProxyModel>;
template class NX_UTILS_API ScopedModelOperations<QSortFilterProxyModel>;
