
#include "server_changes_model.h"


#include <helpers/model_change_helper.h>

namespace
{
    enum
    {
        kSuccesfulModelColsCount = 3
        , kFailedModelColsCount = 4
    };
    
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1

        , kDataTypeRoleId = kFirstCustomRoleId
        , kValueRoleId
        
        , kLastCustomRoleId        
    };
    
    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kDataTypeRoleId, "dataType");
        roles.insert(kValueRoleId, "roleValue");
        return roles;
    }();
    
    struct RequestResultInfo
    {
        QString name;
        QString value;
        QString errorReason;
    };
    typedef QVector<RequestResultInfo> RequestResults;
}

///

class rtu::ServerChangesModel::Impl : public QObject
{
public:
    Impl(bool successfulRequstsModel
        , ModelChangeHelper *helper
        , rtu::ServerChangesModel *owner
        , QObject *parent);
    
    virtual ~Impl();
    
public:
    void addRequestResult(const QString &request
        , const QString &value
        , const QString &errorReason = QString());
    
    int columnsCount() const;
    
    int changesCount() const;
    
    int rowCount() const;
    
    QVariant data(const QModelIndex &index
        , int role) const;
  
private:
    rtu::ServerChangesModel * const m_owner;
    const bool m_successfulRequstsModel;
    const int m_columnsCount;
    ModelChangeHelper * const m_helper;
    RequestResults m_requestResults;
};

///

rtu::ServerChangesModel::Impl::Impl(bool successfulRequstsModel
    , ModelChangeHelper *helper
    , rtu::ServerChangesModel *owner
    , QObject *parent)
    : QObject(parent)
    , m_owner(owner)
    , m_successfulRequstsModel(successfulRequstsModel)
    , m_columnsCount(successfulRequstsModel ? kSuccesfulModelColsCount : kFailedModelColsCount)
    , m_helper(helper)
    , m_requestResults()
{
}

rtu::ServerChangesModel::Impl::~Impl()
{
}

void rtu::ServerChangesModel::Impl::addRequestResult(const QString &request
    , const QString &value
    , const QString &errorReason)
{
    const RequestResultInfo newRequest = { request, value, errorReason };
    
    
    auto it = std::upper_bound(m_requestResults.begin(), m_requestResults.end(), newRequest
        , [&request](const RequestResultInfo &first, const RequestResultInfo &second)
    {
        return (first.name < second.name);
    });
    
    const int newIndex = it - m_requestResults.begin();
    const ModelChangeHelper::Guard actionGuard = m_helper->insertRowsGuard(newIndex, newIndex);
    
    m_requestResults.insert(it, newRequest);
    
    emit m_owner->changesCountChanged();
}

int rtu::ServerChangesModel::Impl::columnsCount() const
{
    return m_columnsCount;
}

int rtu::ServerChangesModel::Impl::changesCount() const
{
    return rowCount();
}

int rtu::ServerChangesModel::Impl::rowCount() const
{
    return m_requestResults.size() * m_columnsCount;
}

QVariant rtu::ServerChangesModel::Impl::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();
    if ((row < 0) || (row >= rowCount()) 
        || (role < kFirstCustomRoleId) || (role > kLastCustomRoleId))
    {
        return QVariant();
    }
    
    const int infoIndex = row / m_columnsCount;
    const int type = row % m_columnsCount;
    
    const RequestResultInfo &info = m_requestResults.at(infoIndex);
    switch(role)
    {
    case kDataTypeRoleId:
        return type;
    case kValueRoleId:
    {
        switch(type)
        {
        case kRequestCaption:
            return info.name;
        case kRequestValue:
            return info.value;
        case kRequestErrorReason:
            return info.errorReason;
        case kRequestSuccessful:
            return m_successfulRequstsModel;
        }
    }
    default:
        return QVariant();
    };
}

///

rtu::ServerChangesModel::ServerChangesModel(bool successfulRequstsModel
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(successfulRequstsModel, CREATE_MODEL_CHANGE_HELPER(this), this, this))
{
    
}

rtu::ServerChangesModel::~ServerChangesModel()
{
}

void rtu::ServerChangesModel::addRequestResult(const QString &request
    , const QString &value
    , const QString &errorReason)
{
    m_impl->addRequestResult(request, value, errorReason);
}

int rtu::ServerChangesModel::columnsCount() const
{
    return m_impl->columnsCount();
}

int rtu::ServerChangesModel::changesCount() const
{
    return m_impl->changesCount();
}

int rtu::ServerChangesModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_impl->rowCount();
}

QVariant rtu::ServerChangesModel::data(const QModelIndex &index
    , int role) const
{
    return m_impl->data(index, role);
}

rtu::Roles rtu::ServerChangesModel::roleNames() const
{
    return kRoles;
}

