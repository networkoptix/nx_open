#pragma once

#include <type_traits>
#include <QtCore/QAbstractItemModel>

#include <nx/utils/scope_guard.h>
#include <utils/common/forward.h>

namespace detail {

class ScopeGuardWithInitializationFunc:
    public nx::utils::ScopeGuard<std::function<void()>>
{
    using base_type = nx::utils::ScopeGuard<std::function<void()>>;

public:
    ScopeGuardWithInitializationFunc() = default;
    ScopeGuardWithInitializationFunc(ScopeGuardWithInitializationFunc&&) = default;
    ScopeGuardWithInitializationFunc(const ScopeGuardWithInitializationFunc&) = default;

    /**
     * @param initializationFunc Called right in this constructor.
     */
    ScopeGuardWithInitializationFunc(
        std::function<void()> initializationFunc,
        std::function<void()> guardFunc)
        :
        base_type(std::move(guardFunc))
    {
        if (initializationFunc)
            initializationFunc();
    }

    ScopeGuardWithInitializationFunc(std::function<void()> guardFunc):
        base_type(std::move(guardFunc))
    {
    }
};

} // namespace detail

//-------------------------------------------------------------------------------------------------

template<class BaseModel>
class ScopedModelOperations: public BaseModel
{
    static_assert(std::is_base_of<QAbstractItemModel, BaseModel>::value,
        "BaseModel must be derived from QAbstractItemModel");

public:
    QN_FORWARD_CONSTRUCTOR(ScopedModelOperations, BaseModel, {});

protected:
    class ScopedReset final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        explicit ScopedReset(ScopedModelOperations* model, bool condition = true):
            detail::ScopeGuardWithInitializationFunc(std::move(condition
                ? detail::ScopeGuardWithInitializationFunc(
                    [model]() { model->beginResetModel(); },
                    [model]() { model->endResetModel(); })
                : detail::ScopeGuardWithInitializationFunc([]() {})))
        {
        }
    };

    class ScopedInsertColumns final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedInsertColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            detail::ScopeGuardWithInitializationFunc(
                [model, parent, first, last]() { model->beginInsertColumns(parent, first, last); },
                [model]() { model->endInsertColumns(); })
        {
        }

        ScopedInsertColumns(ScopedModelOperations* model, int first, int last):
            ScopedInsertColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedInsertRows final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedInsertRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            detail::ScopeGuardWithInitializationFunc(
                [model, parent, first, last]() { model->beginInsertRows(parent, first, last); },
                [model]() { model->endInsertRows(); })
        {
        }

        ScopedInsertRows(ScopedModelOperations* model, int first, int last):
            ScopedInsertRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveColumns final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedRemoveColumns(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            detail::ScopeGuardWithInitializationFunc(
                [model, parent, first, last]() { model->beginRemoveColumns(parent, first, last); },
                [model]() { model->endRemoveColumns(); })
        {
        }

        ScopedRemoveColumns(ScopedModelOperations* model, int first, int last):
            ScopedRemoveColumns(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedRemoveRows final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedRemoveRows(ScopedModelOperations* model, const QModelIndex& parent,
            int first, int last)
            :
            detail::ScopeGuardWithInitializationFunc(
                [model, parent, first, last]() { model->beginRemoveRows(parent, first, last); },
                [model]() { model->endRemoveRows(); })
        {
        }

        ScopedRemoveRows(ScopedModelOperations* model, int first, int last):
            ScopedRemoveRows(model, QModelIndex(), first, last)
        {
        }
    };

    class ScopedMoveColumns final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedMoveColumns(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            detail::ScopeGuardWithInitializationFunc(
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

    class ScopedMoveRows final: detail::ScopeGuardWithInitializationFunc
    {
    public:
        ScopedMoveRows(ScopedModelOperations* model, const QModelIndex& sourceParent,
            int sourceFirst, int sourceLast, const QModelIndex& destinationParent,
            int destinationPos)
            :
            detail::ScopeGuardWithInitializationFunc(
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
