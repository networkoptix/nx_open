
#include "changes_summary_model.h"

#include <helpers/model_change_helper.h>

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
    
        , kIdRoleId = kFirstCustomRoleId
        
        , kRequestNameRoleId
        , kValueRoleId
        , kErrorReasonRoleId
        , kSuccessRoleId
        
        , kLastCustomRoleId        
    };
    
    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kIdRoleId, "id");
        roles.insert(kRequestNameRoleId, "name");
        roles.insert(kValueRoleId, "value");
        roles.insert(kErrorReasonRoleId, "reason");
        roles.insert(kSuccessRoleId, "success");
        return roles;
    }();

    struct RequestResultInfo
    {
        QString id;
        QString name;
        QString value;
        QString errorReason;
        
        RequestResultInfo();
        
        RequestResultInfo(const QString &initId
            , const QString &initName
            , const QString &initValue
            , const QString &initErrorReason);
    };
    
    RequestResultInfo::RequestResultInfo()
        : id()
        , name()
        , value()
        , errorReason()
    {}
    
    RequestResultInfo::RequestResultInfo(const QString &initId
        , const QString &initName
        , const QString &initValue
        , const QString &initErrorReason)
        : id(initId)
        , name(initName)
        , value(initValue)
        , errorReason(initErrorReason)
    {}
    
    typedef QVector<RequestResultInfo> RequestResults;
}


class rtu::ChangesSummaryModel::Impl : public QObject
{
public:
    Impl(bool successfulChangesModel
        , ModelChangeHelper * const changeHelper
        , rtu::ChangesSummaryModel *owner);
    
    virtual ~Impl();
    
public:
    void addRequestResult(const ServerInfo &info
        , const QString &request
        , const QString &value
        , const QString &errorReason);
    
public:
    bool isSuccessChangesModel() const;
    
    int rowCount() const;

    QVariant data(const QModelIndex &index
        , int role) const;
    
private:
    const bool m_successfulChangesModel;
    rtu::ChangesSummaryModel * const m_owner;
    rtu::ModelChangeHelper * const m_changeHelper;
    
    RequestResults m_results;
};

rtu::ChangesSummaryModel::Impl::Impl(bool successfulChangesModel
    , ModelChangeHelper * const changeHelper
    , rtu::ChangesSummaryModel *owner)
    : QObject(owner)
    , m_successfulChangesModel(successfulChangesModel)
    , m_owner(owner)
    , m_changeHelper(changeHelper)
    
    , m_results()
{   
}

rtu::ChangesSummaryModel::Impl::~Impl() 
{
}

void rtu::ChangesSummaryModel::Impl::addRequestResult(const ServerInfo &info
    , const QString &request
    , const QString &value
    , const QString &errorReason)
{
    static const QString kTemplate = "%1%2 (%3)";
    const QString address = (info.baseInfo().displayAddress.isEmpty() 
        ? info.baseInfo().hostAddress : info.baseInfo().displayAddress);
    const QString &newId = kTemplate.arg(info.baseInfo().id.toString()
        , info.baseInfo().name, address);
    const RequestResults::iterator it = std::upper_bound(m_results.begin(), m_results.end(), newId
        , [](const QString &nId, const RequestResultInfo &resultInfo)
    {
        return  (nId < resultInfo.id);
    });
    
    const int newIndex = (it == m_results.end() 
        ? m_results.size() : it - m_results.begin());
    
    const ModelChangeHelper::Guard actionGuard 
        = m_changeHelper->insertRowsGuard(newIndex, newIndex);
    Q_UNUSED(actionGuard);
    
    m_results.insert(it, RequestResultInfo(newId, request, value, errorReason));
    emit m_owner->changesCountChanged();
}

bool rtu::ChangesSummaryModel::Impl::isSuccessChangesModel() const
{
    return m_successfulChangesModel;
}

int rtu::ChangesSummaryModel::Impl::rowCount() const
{
    return m_results.size();
}

QVariant rtu::ChangesSummaryModel::Impl::data(const QModelIndex &index
    , int role) const
{
    const int row = index.row();
    if ((row < 0) || (row >= rowCount()) 
        || (role < kFirstCustomRoleId) || (role > kLastCustomRoleId))
    {
        return QVariant();
    }

    const RequestResultInfo &info = m_results.at(row);
    switch(role)    
    {
    case kIdRoleId:
        return info.id;
    case kRequestNameRoleId:
        return info.name;
    case kValueRoleId:
        return info.value;
    case kErrorReasonRoleId:
        return info.errorReason;
    case kSuccessRoleId:
        return m_successfulChangesModel;
    default:
        return QVariant();
    }
}

/// 

rtu::ChangesSummaryModel::ChangesSummaryModel(bool successfulChangesModel
    , QObject *parent)
    : QAbstractListModel(parent)
    , m_impl(new Impl(successfulChangesModel, CREATE_MODEL_CHANGE_HELPER(this), this))
{
    
}

rtu::ChangesSummaryModel::~ChangesSummaryModel()
{
    
}

void rtu::ChangesSummaryModel::addRequestResult(const ServerInfo &info
    , const QString &request
    , const QString &value
    , const QString &errorReason)
{
    m_impl->addRequestResult(info, request, value, errorReason);
}

int rtu::ChangesSummaryModel::changesCount() const
{
    return m_impl->rowCount();
}

bool rtu::ChangesSummaryModel::isSuccessChangesModel() const
{
    return m_impl->isSuccessChangesModel();
}

int rtu::ChangesSummaryModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_impl->rowCount();
}

QVariant rtu::ChangesSummaryModel::data(const QModelIndex &index
    , int role) const
{
    return m_impl->data(index, role);
}

rtu::Roles rtu::ChangesSummaryModel::roleNames() const
{
    return kRoles;
}

