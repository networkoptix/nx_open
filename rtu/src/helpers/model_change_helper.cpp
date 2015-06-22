
#include "model_change_helper.h"

class rtu::ModelChangeHelper::ChangeGuard
{
public:
    ChangeGuard(const ActionFunction &endAction);
    
    ~ChangeGuard();
    
private:
    const rtu::ModelChangeHelper::ActionFunction m_endAction;
};

rtu::ModelChangeHelper::ChangeGuard::ChangeGuard(const ActionFunction &endAction)
    : m_endAction(endAction)
{
}

rtu::ModelChangeHelper::ChangeGuard::~ChangeGuard()
{
    m_endAction();
}


///

rtu::ModelChangeHelper::ModelChangeHelper(const RowsActionFunction &beginInsertRows
    , const ActionFunction &endInsertRows
    , const RowsActionFunction &beginRemoveRows
    , const ActionFunction &endRemoveRows
    , const ActionFunction &beginResetModel
    , const ActionFunction &endResetModel
    , const RowsActionFunction &dataChangedFunction
    , QObject *parent)
    : QObject(parent)
    , m_beginInsertRows(beginInsertRows)
    , m_endInsertRows(endInsertRows)
    , m_beginRemoveRows(beginRemoveRows)
    , m_endRemoveRows(endRemoveRows)
    , m_beginResetModel(beginResetModel)
    , m_endResetModel(endResetModel)
    , m_dataChangedFunction(dataChangedFunction)
{
}

rtu::ModelChangeHelper::~ModelChangeHelper()
{
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::insertRowsGuard(int startIndex
    , int finishIndex)
{
    m_beginInsertRows(startIndex, finishIndex);
    return Guard(new ChangeGuard(m_endInsertRows));
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::removeRowsGuard(int startIndex
    , int finishIndex)
{
    m_beginRemoveRows(startIndex, finishIndex);
    return Guard(new ChangeGuard(m_endRemoveRows));
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::resetModelGuard()
{
    m_beginResetModel();
    return Guard(new ChangeGuard(m_endResetModel));
}

void rtu::ModelChangeHelper::dataChanged(int startIndex
    , int finishIndex)
{
    m_dataChangedFunction(startIndex, finishIndex);
}
