
#include "server_info_manager.h"

#include <functional>

#include <QStringList>
#include <QElapsedTimer>

namespace api = nx::vms::server::api;

namespace
{
    enum { kShortTimeout = 3000};

    QStringList g_availablePasswords = api::Client::defaultAdminPasswords();

    const auto globalSuccessful = [](const rtu::ServerInfoManager::SuccessfulCallback &callback
        , const QUuid &id, const api::ExtraServerInfo &extraInfo, const QString &host)
    {
        if (!g_availablePasswords.contains(extraInfo.password))
            g_availablePasswords.push_back(extraInfo.password);

        if (callback)
            callback(id, extraInfo, host);
    };
}

class rtu::ServerInfoManager::Impl : public QObject
{
public:
    Impl(ServerInfoManager *parent);
    
    virtual ~Impl();
    
    void primaryLoginToServer(const api::BaseServerInfoPtr &info
        , const QString &password
        , const ServerInfoManager::SuccessfulCallback &successful
        , const ServerInfoManager::FailedCallback &failed);
    
    void loginToServer(const api::BaseServerInfoPtr &info
        , int passwordIndex
        , const ServerInfoManager::SuccessfulCallback &successful
        , const ServerInfoManager::FailedCallback &failed);    
    ///

    void updateServerInfos(const ServerInfoContainer &servers
        , const ServerInfoManager::SuccessfulCallback &successful
        , const ServerInfoManager::FailedCallback &failed);

private:
    typedef QHash<QUuid, qint64> LastUpdateTimeMap;

    ServerInfoManager * const m_owner;

    QElapsedTimer m_msCounter;
    LastUpdateTimeMap m_lastUpdated;
};

rtu::ServerInfoManager::Impl::Impl(ServerInfoManager *parent)
    : QObject(parent)
    , m_owner(parent)
    
    , m_msCounter()
    , m_lastUpdated()
{
    m_msCounter.start();
}

rtu::ServerInfoManager::Impl::~Impl()
{
}

void rtu::ServerInfoManager::Impl::primaryLoginToServer(const api::BaseServerInfoPtr &info
    , const QString &password
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info->hostAddress);
    };

    const auto &localFailed = [this, info, failed](const api::Client::ResultCode errorCode, int)
    {
        if (failed)
            failed(info->id, errorCode);
    };
    
    api::Client::getServerExtraInfo(info, password, localSuccessful, localFailed, kShortTimeout);
} 

void rtu::ServerInfoManager::Impl::loginToServer(const api::BaseServerInfoPtr &info
    , int passwordIndex
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    if (passwordIndex >= g_availablePasswords.size())
    {
        if (failed)
            failed(info->id, api::Client::ResultCode::kUnauthorized);
        return;
    }
    
    const auto &localFailed = 
        [this, info, passwordIndex, successful, failed](const api::Client::ResultCode errorCode, int)
    {
        if (errorCode == api::Client::ResultCode::kUnauthorized)
        {
            loginToServer(info, passwordIndex + 1, successful, failed);
            return;
        }

        if (failed)
            failed(info->id, api::Client::ResultCode::kUnspecified);
    };
    
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info->hostAddress);
    };

    api::Client::getServerExtraInfo(info
        , g_availablePasswords.at(passwordIndex), localSuccessful, localFailed, kShortTimeout);
}

void rtu::ServerInfoManager::Impl::updateServerInfos(const ServerInfoContainer &servers
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{

    for (const api::ServerInfo &server: servers)
    {
        const api::BaseServerInfoPtr &base = std::make_shared<BaseServerInfo>(server.baseInfo());
        const int timestamp = m_msCounter.elapsed();
        const auto &localSuccessful = 
            [this, base, timestamp, successful](const QUuid &, const ExtraServerInfo &extraInfo)
        {
            const auto callback = [this, base, timestamp, successful]
                (const QUuid &id, const ExtraServerInfo &extraInfo, const QString &host)
            {
                m_lastUpdated[id] = timestamp;
                if (successful)
                    successful(id, extraInfo, host);
            };

            globalSuccessful(callback, base->id, extraInfo, base->hostAddress);
        };

        const auto &localFailed = 
            [this, base, successful, timestamp, failed](const api::Client::ResultCode /* errorCode */, int /* affected */) 
        {
            /// on fail - try re-login
            const auto &loginFailed = [this, timestamp, failed](const QUuid &id, api::Client::ResultCode errorCode)
            {
                if (m_lastUpdated[id] <= timestamp)
                    failed(id, errorCode);
            };

            loginToServer(base, 0, successful, loginFailed);
        };

        const QString password = (server.hasExtraInfo() ? server.extraInfo().password 
            : api::Client::defaultAdminPasswords().front());

        api::Client::getServerExtraInfo(base, password, localSuccessful, localFailed, api::Client::kDefaultTimeoutMs);
    }
}

///

rtu::ServerInfoManager::ServerInfoManager(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
}

rtu::ServerInfoManager::~ServerInfoManager()
{   
}

void rtu::ServerInfoManager::loginToServer(const BaseServerInfo &info
    , const SuccessfulCallback &success
    , const FailedCallback &failed)
{
    m_impl->loginToServer(std::make_shared<BaseServerInfo>(info), 0, success, failed);
}

void rtu::ServerInfoManager::loginToServer(const BaseServerInfo &info
    , const QString &password
    , const SuccessfulCallback &success
    , const FailedCallback &failed)
{
    m_impl->primaryLoginToServer(std::make_shared<BaseServerInfo>(info), password, success, failed);
}

void rtu::ServerInfoManager::updateServerInfos(const ServerInfoContainer &servers
    , const SuccessfulCallback &success
    , const FailedCallback &failed)
{
    m_impl->updateServerInfos(servers, success, failed);
}

