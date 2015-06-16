
#include "model_change_helper.h"

class rtu::ModelChangeHelper::ChangeGuard
{
public:
    ChangeGuard(const EndActionFunction &endAction);
    
    ~ChangeGuard();
    
private:
    const rtu::ModelChangeHelper::EndActionFunction m_endAction;
};

rtu::ModelChangeHelper::ChangeGuard::ChangeGuard(const EndActionFunction &endAction)
    : m_endAction(endAction)
{
}

rtu::ModelChangeHelper::ChangeGuard::~ChangeGuard()
{
    m_endAction();
}


///

rtu::ModelChangeHelper::ModelChangeHelper(const RowsActionFunction &beginInsertRows
    , const EndActionFunction &endInsertRows
    , const RowsActionFunction &beginRemoveRows
    , const EndActionFunction &endRemoveRows
    , const RowsActionFunction &dataChangedFunction
    , QObject *parent)
    : QObject(parent)
    , m_beginInsertRows(beginInsertRows)
    , m_endInsertRows(endInsertRows)
    , m_beginRemoveRows(beginRemoveRows)
    , m_endRemoveRows(endRemoveRows)
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

void rtu::ModelChangeHelper::dataChanged(int startIndex
    , int finishIndex)
{
    m_dataChangedFunction(startIndex, finishIndex);
}
