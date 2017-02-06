
#pragma once

#include <QObject>
#include <QSharedPointer>

#include <functional>

class QModelIndex;

namespace rtu
{
    class ModelChangeHelper : public QObject
    {
        typedef std::function<void (int, int)> RowsActionFunction;
        typedef std::function<void ()> ActionFunction;
        
    public:
        class ChangeGuard;
        typedef QSharedPointer<ChangeGuard> Guard;
        
        ModelChangeHelper(const RowsActionFunction &beginInsertRows
            , const ActionFunction &endInsertRows
            , const RowsActionFunction &beginRemoveRows
            , const ActionFunction &endRemoveRows
            , const ActionFunction &beginResetModel
            , const ActionFunction &endResetModel
            , const RowsActionFunction &dataChangedFunction
            , const ActionFunction &layoutChanged
            , QObject *parent);
        
        virtual ~ModelChangeHelper();
        
        Guard insertRowsGuard(int startIndex
            , int finishIndex) const;
        
        Guard removeRowsGuard(int startIndex
            , int finishIndex) const;
        
        Guard resetModelGuard() const;
        
        void dataChanged(int startIndex
            , int finishIndex) const;
        
    private:
        const RowsActionFunction m_beginInsertRows;
        const ActionFunction m_endInsertRows;
        const RowsActionFunction m_beginRemoveRows;
        const ActionFunction m_endRemoveRows;
        const ActionFunction m_beginResetModel;
        const ActionFunction m_endResetModel;
        const RowsActionFunction m_dataChangedFunction;
        const ActionFunction m_layoutChanged;
    };

}

#define CREATE_MODEL_CHANGE_HELPER(parent)                                      \
    new ModelChangeHelper(                                                      \
        [this](int startIndex, int finishIndex)                                 \
            { beginInsertRows(QModelIndex(), startIndex, finishIndex); }        \
        , [this]() { endInsertRows(); }                                         \
        , [this](int startIndex, int finishIndex)                               \
            { beginRemoveRows(QModelIndex(), startIndex, finishIndex); }        \
        , [this]() { endRemoveRows(); }                                         \
        , [this]() { beginResetModel(); }                                       \
        , [this]() { endResetModel(); }                                         \
        , [this](int startIndex, int finishIndex)                               \
            { emit dataChanged(index(startIndex), index(finishIndex)); }        \
        , [this]() { emit layoutChanged(); }                                    \
        , parent)
