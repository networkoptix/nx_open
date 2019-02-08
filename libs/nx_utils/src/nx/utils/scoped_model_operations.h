#pragma once

#include <type_traits>
#include <QtCore/QAbstractItemModel>


#include <nx/utils/scope_guard.h>

template<class BaseModel>
class ScopedModelOperations: public BaseModel
{
    static_assert(std::is_base_of<QAbstractItemModel, BaseModel>::value,
        "BaseModel must be derived from QAbstractItemModel");

public:
    using BaseModel::BaseModel;

protected:
    class ScopedReset: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        explicit ScopedReset(ScopedModelOperations* model, bool condition = true):
            base_type(condition
                ? nx::utils::SharedGuardCallback([model]() { model->endResetModel(); })
                : nx::utils::SharedGuardCallback([]() {}))
        {
            if (condition)
                model->beginResetModel();
        }
    };

    class ScopedInsertColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedInsertColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            base_type(std::move([model]() { model->endInsertColumns(); }))
        {
            model->beginInsertColumns(parent, first, last);
        }

        ScopedInsertColumns(ScopedModelOperations* model, int first, int last):
            ScopedInsertColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedInsertRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedInsertRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            base_type(std::move([model]() { model->endInsertRows(); }))
        {
            model->beginInsertRows(parent, first, last);
        }

        ScopedInsertRows(ScopedModelOperations* model, int first, int last):
            ScopedInsertRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedRemoveColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            base_type(std::move([model]() { model->endRemoveColumns(); }))
        {
            model->beginRemoveColumns(parent, first, last);
        }

        ScopedRemoveColumns(ScopedModelOperations* model, int first, int last):
            ScopedRemoveColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedRemoveRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            base_type(std::move([model]() { model->endRemoveRows(); }))
        {
            model->beginRemoveRows(parent, first, last);
        }

        ScopedRemoveRows(ScopedModelOperations* model, int first, int last):
            ScopedRemoveRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedMoveColumns: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedMoveColumns(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            base_type(std::move([model]() { model->endMoveColumns(); }))
        {
            model->beginMoveColumns(sourceParent, sourceFirst, sourceLast,
                destinationParent, destinationPos);
        }

        ScopedMoveColumns(ScopedModelOperations* model, int sourceFirst, int sourceLast,
            int destinationPos)
            :
            ScopedMoveColumns(model, QModelIndex(), sourceFirst, sourceLast,
                QModelIndex(), destinationPos)
        {
        }
    };

    class ScopedMoveRows: public nx::utils::SharedGuard
    {
        using base_type = nx::utils::SharedGuard;
    public:
        ScopedMoveRows(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            base_type(std::move([model]() { model->endMoveRows(); }))
        {
            model->beginMoveRows(sourceParent, sourceFirst, sourceLast,
                destinationParent, destinationPos);
        }

        ScopedMoveRows(ScopedModelOperations* model, int sourceFirst, int sourceLast,
            int destinationPos)
            :
            ScopedMoveRows(model, QModelIndex(), sourceFirst, sourceLast,
                QModelIndex(), destinationPos)
        {
        }
    };
};
