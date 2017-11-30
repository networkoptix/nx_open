#pragma once

#include <type_traits>
#include <QtCore/QAbstractItemModel>

#include <nx/utils/raii_guard.h>
#include <utils/common/forward.h>


template<class BaseModel>
class ScopedModelOperations : public BaseModel
{
    static_assert(std::is_base_of<QAbstractItemModel, BaseModel>::value,
        "BaseModel must be derived from QAbstractItemModel");

public:
    QN_FORWARD_CONSTRUCTOR(ScopedModelOperations, BaseModel, {});

protected:
    class ScopedReset final : QnRaiiGuard
    {
    public:
        explicit ScopedReset(ScopedModelOperations* model, bool condition = true):
            QnRaiiGuard(std::move(condition
                ? QnRaiiGuard(
                    [model]() { model->beginResetModel(); },
                    [model]() { model->endResetModel(); })
                : QnRaiiGuard([]() {})))
        {
        }
    };

    class ScopedInsertColumns final : QnRaiiGuard
    {
    public:
        ScopedInsertColumns(ScopedModelOperations* model, const QModelIndex& parent, int first, int last) :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginInsertColumns(parent, first, last); },
                [model]() { model->endInsertColumns(); })
        {
        }
    };

    class ScopedInsertRows final : QnRaiiGuard
    {
    public:
        ScopedInsertRows(ScopedModelOperations* model, const QModelIndex& parent, int first, int last) :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginInsertRows(parent, first, last); },
                [model]() { model->endInsertRows(); })
        {
        }
    };

    class ScopedRemoveColumns final : QnRaiiGuard
    {
    public:
        ScopedRemoveColumns(ScopedModelOperations* model, const QModelIndex& parent, int first, int last) :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginRemoveColumns(parent, first, last); },
                [model]() { model->endRemoveColumns(); })
        {
        }
    };

    class ScopedRemoveRows final : QnRaiiGuard
    {
    public:
        ScopedRemoveRows(ScopedModelOperations* model, const QModelIndex& parent, int first, int last) :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginRemoveRows(parent, first, last); },
                [model]() { model->endRemoveRows(); })
        {
        }
    };

    class ScopedMoveColumns final : QnRaiiGuard
    {
    public:
        ScopedMoveColumns(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationChild) :
                QnRaiiGuard(
                    [model, sourceParent, sourceFirst, sourceLast, destinationParent, destinationChild]()
                    {
                        model->beginMoveColumns(sourceParent, sourceFirst, sourceLast,
                            destinationParent, destinationChild);
                    },
                    [model]()
                    {
                        model->endMoveColumns();
                    })
        {
        }
    };

    class ScopedMoveRows final : QnRaiiGuard
    {
    public:
        ScopedMoveRows(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent, int destinationChild) :
                QnRaiiGuard(
                    [model, sourceParent, sourceFirst, sourceLast, destinationParent, destinationChild]()
                    {
                        model->beginMoveRows(sourceParent, sourceFirst, sourceLast,
                            destinationParent, destinationChild);
                    },
                    [model]()
                    {
                        model->endMoveRows();
                    })
        {
        }
    };
};
