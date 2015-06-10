
#include "changes_summary_model.h"

#include <base/server_info.h>
#include <models/server_changes_model.h>
#include <helpers/model_change_helper.h>

namespace
{
    enum
    {
        kFirstCustomRoleId = Qt::UserRole + 1
    
        , kServerName = kFirstCustomRoleId
        , kResultsModel
        
        , kLastCustomRoleId        
    };
    
    const rtu::Roles kRoles = []() -> rtu::Roles
    {
        rtu::Roles roles;
        roles.insert(kServerName, "serverName");
        roles.insert(kResultsModel, "resultsModel");
        return roles;
    }();

}

namespace
{
    struct ServerRequestsInfo
    {
        rtu::ServerInfo info;
        rtu::ServerChangesModel *changesModel;

        ServerRequestsInfo();

        ServerRequestsInfo(const rtu::ServerInfo &initInfo
            , rtu::ServerChangesModel *initChangesModel);
    };

    ServerRequestsInfo::ServerRequestsInfo()
        : info()
        , changesModel(nullptr)
    {}

    ServerRequestsInfo::ServerRequestsInfo(const rtu::ServerInfo &initInfo
        , rtu::ServerChangesModel *initChangesModel)
        : info(initInfo)
        , changesModel(initChangesModel) 
    {}

    typedef QVector<ServerRequestsInfo> ServersRequests;
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
    int rowCount() const;

    QVariant data(const QModelIndex &index
        , int role) const;
    
    
private:
    const bool m_successfulChangesModel;
    rtu::ChangesSummaryModel * const m_owner;
    rtu::ModelChangeHelper * const m_changeHelper;
    
    ServersRequests m_requests;
};

rtu::ChangesSummaryModel::Impl::Impl(bool successfulChangesModel
    , ModelChangeHelper * const changeHelper
    , rtu::ChangesSummaryModel *owner)
    : QObject(owner)
    , m_successfulChangesModel(successfulChangesModel)
    , m_owner(owner)
    , m_changeHelper(changeHelper)
    
    , m_requests()
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
    auto it = std::find_if(m_requests.begin(), m_requests.end()
        , [info](const ServerRequestsInfo &requestInfo) 
    {
        return requestInfo.info.baseInfo().id == info.baseInfo().id;
    });
    
    if (it == m_requests.end())
    {
        int newIndex = rowCount();
        const ModelChangeHelper::Guard modelChangeAction = m_changeHelper->insertRowsGuard(newIndex, newIndex);
        it = m_requests.insert(m_requests.end(), ServerRequestsInfo(info, new ServerChangesModel(m_successfulChangesModel, this)));

        Q_UNUSED(modelChangeAction);
    }
    
    it->changesModel->addRequestResult(request, value, errorReason);
}

int rtu::ChangesSummaryModel::Impl::rowCount() const
{
    return m_requests.size();
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

    const ServerRequestsInfo &info = m_requests.at(row);
    switch(role)    
    {
    case kServerName:
        return info.info.baseInfo().name;
    case kResultsModel:
        return QVariant::fromValue(info.changesModel);
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

