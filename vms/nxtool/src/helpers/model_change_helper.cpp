
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
    , const ActionFunction &layoutChanged
    , QObject *parent)
    : QObject(parent)
    , m_beginInsertRows(beginInsertRows)
    , m_endInsertRows(endInsertRows)
    , m_beginRemoveRows(beginRemoveRows)
    , m_endRemoveRows(endRemoveRows)
    , m_beginResetModel(beginResetModel)
    , m_endResetModel(endResetModel)
    , m_dataChangedFunction(dataChangedFunction)
    , m_layoutChanged(layoutChanged)
{
}

rtu::ModelChangeHelper::~ModelChangeHelper()
{
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::insertRowsGuard(int startIndex
    , int finishIndex) const
{
    m_beginInsertRows(startIndex, finishIndex);
    return Guard(new ChangeGuard([this]()
    {
        if (m_endInsertRows)
            m_endInsertRows();
        if (m_layoutChanged)
            m_layoutChanged();
    }));
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::removeRowsGuard(int startIndex
    , int finishIndex) const
{
    m_beginRemoveRows(startIndex, finishIndex);
    return Guard(new ChangeGuard([this]()
    {
        if (m_endRemoveRows)
            m_endRemoveRows();
        if (m_layoutChanged)
            m_layoutChanged();
    }));
}

rtu::ModelChangeHelper::Guard rtu::ModelChangeHelper::resetModelGuard() const
{
    m_beginResetModel();
    return Guard(new ChangeGuard(m_endResetModel));
}

void rtu::ModelChangeHelper::dataChanged(int startIndex
    , int finishIndex) const
{
    m_dataChangedFunction(startIndex, finishIndex);
}
