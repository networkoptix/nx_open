#pragma once

#include <type_traits>
#include <QtCore/QAbstractItemModel>

#include <nx/utils/raii_guard.h>
#include <utils/common/forward.h>


template<class BaseModel>
class ScopedModelOperations: public BaseModel
{
    static_assert(std::is_base_of<QAbstractItemModel, BaseModel>::value,
        "BaseModel must be derived from QAbstractItemModel");

public:
    QN_FORWARD_CONSTRUCTOR(ScopedModelOperations, BaseModel, {});

protected:
    class ScopedReset final: QnRaiiGuard
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

    class ScopedInsertColumns final: QnRaiiGuard
    {
    public:
        ScopedInsertColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginInsertColumns(parent, first, last); },
                [model]() { model->endInsertColumns(); })
        {
        }

        ScopedInsertColumns(ScopedModelOperations* model, int first, int last):
            ScopedInsertColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedInsertRows final: QnRaiiGuard
    {
    public:
        ScopedInsertRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginInsertRows(parent, first, last); },
                [model]() { model->endInsertRows(); })
        {
        }

        ScopedInsertRows(ScopedModelOperations* model, int first, int last):
            ScopedInsertRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveColumns final: QnRaiiGuard
    {
    public:
        ScopedRemoveColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginRemoveColumns(parent, first, last); },
                [model]() { model->endRemoveColumns(); })
        {
        }

        ScopedRemoveColumns(ScopedModelOperations* model, int first, int last):
            ScopedRemoveColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveRows final: QnRaiiGuard
    {
    public:
        ScopedRemoveRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            QnRaiiGuard(
                [model, parent, first, last]() { model->beginRemoveRows(parent, first, last); },
                [model]() { model->endRemoveRows(); })
        {
        }

        ScopedRemoveRows(ScopedModelOperations* model, int first, int last):
            ScopedRemoveRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedMoveColumns final: QnRaiiGuard
    {
    public:
        ScopedMoveColumns(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            QnRaiiGuard(
                [model, sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos]()
                {
                    model->beginMoveColumns(sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos);
                },
                [model]()
                {
                    model->endMoveColumns();
                })
        {
        }

        ScopedMoveColumns(ScopedModelOperations* model, int sourceFirst, int sourceLast,
            int destinationPos)
            :
            ScopedMoveColumns(model, QModelIndex(), sourceFirst, sourceLast,
                QModelIndex(), destinationPos)
        {
        }
    };

    class ScopedMoveRows final: QnRaiiGuard
    {
    public:
        ScopedMoveRows(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            QnRaiiGuard(
                [model, sourceParent, sourceFirst, sourceLast, destinationParent, destinationPos]()
                {
                    model->beginMoveRows(sourceParent, sourceFirst, sourceLast,
                        destinationParent, destinationPos);
                },
                [model]()
                {
                    model->endMoveRows();
                })
        {
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
