
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
        typedef std::function<void ()> EndActionFunction;
        
    public:
        class ChangeGuard;
        typedef QSharedPointer<ChangeGuard> Guard;
        
        ModelChangeHelper(const RowsActionFunction &beginInsertRows
            , const EndActionFunction &endInsertRows
            , const RowsActionFunction &beginRemoveRows
            , const EndActionFunction &endRemoveRows
            , const RowsActionFunction &dataChangedFunction
            , QObject *parent);
        
        virtual ~ModelChangeHelper();
        
        Guard insertRowsGuard(int startIndex
            , int finishIndex);
        
        Guard removeRowsGuard(int startIndex
            , int finishIndex);
        
        void dataChanged(int startIndex
            , int finishIndex);
        
    private:
        const RowsActionFunction m_beginInsertRows;
        const EndActionFunction m_endInsertRows;
        const RowsActionFunction m_beginRemoveRows;
        const EndActionFunction m_endRemoveRows;
        const RowsActionFunction m_dataChangedFunction;
    };

}

#define CREATE_MODEL_CHANGE_HELPER(parent)                                      \
    new ModelChangeHelper(                                                      \
        [this](int startIndex, int finishIndex)                                 \
            { emit beginInsertRows(QModelIndex(), startIndex, finishIndex); }   \
        , [this]() { emit endInsertRows(); }                                    \
        , [this](int startIndex, int finishIndex)                               \
            { emit beginRemoveRows(QModelIndex(), startIndex, finishIndex); }   \
        , [this]() { emit endRemoveRows(); }                                    \
        , [this](int startIndex, int finishIndex)                               \
            { emit dataChanged(index(startIndex), index(finishIndex)); }        \
        , parent)
