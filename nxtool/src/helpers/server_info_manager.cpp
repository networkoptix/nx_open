
#include "server_info_manager.h"

#include <functional>

#include <QStringList>
#include <QElapsedTimer>

#include <base/requests.h>
#include <base/server_info.h>
#include <helpers/rest_client.h>

namespace
{
    enum { kShortTimeout = 3000};

    QStringList g_availablePasswords = rtu::RestClient::defaultAdminPasswords();

    const auto globalSuccessful = [](const rtu::ServerInfoManager::SuccessfulCallback &callback
        , const QUuid &id, const rtu::ExtraServerInfo &extraInfo, const QString &host)
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
    
    void primaryLoginToServer(const BaseServerInfoPtr &info
        , const QString &password
        , const ServerInfoManager::SuccessfulCallback &successful
        , const ServerInfoManager::FailedCallback &failed);
    
    void loginToServer(const BaseServerInfoPtr &info
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

void rtu::ServerInfoManager::Impl::primaryLoginToServer(const BaseServerInfoPtr &info
    , const QString &password
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info->hostAddress);
    };

    const auto &localFailed = [this, info, failed](const RequestError errorCode, int)
    {
        if (failed)
            failed(info->id, errorCode);
    };
    
    getServerExtraInfo(info, password, localSuccessful, localFailed, kShortTimeout);
} 

void rtu::ServerInfoManager::Impl::loginToServer(const BaseServerInfoPtr &info
    , int passwordIndex
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    if (passwordIndex >= g_availablePasswords.size())
    {
        if (failed)
            failed(info->id, RequestError::kUnauthorized);
        return;
    }
    
    const auto &localFailed = 
        [this, info, passwordIndex, successful, failed](const RequestError errorCode, int)
    {
        if (errorCode == RequestError::kUnauthorized)
        {
            loginToServer(info, passwordIndex + 1, successful, failed);
            return;
        }

        if (failed)
            failed(info->id, RequestError::kUnspecified);
    };
    
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info->hostAddress);
    };

    getServerExtraInfo(info
        , g_availablePasswords.at(passwordIndex), localSuccessful, localFailed, kShortTimeout);
}

void rtu::ServerInfoManager::Impl::updateServerInfos(const ServerInfoContainer &servers
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{

    for (const ServerInfo &server: servers)
    {
        const BaseServerInfoPtr &base = std::make_shared<BaseServerInfo>(server.baseInfo());
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
            [this, base, successful, timestamp, failed](const RequestError /* errorCode */, int /* affected */) 
        {
            /// on fail - try re-login
            const auto &loginFailed = [this, timestamp, failed](const QUuid &id, RequestError errorCode)
            {
                if (m_lastUpdated[id] <= timestamp)
                    failed(id, errorCode);
            };

            loginToServer(base, 0, successful, loginFailed);
        };

        const QString password = (server.hasExtraInfo() ? server.extraInfo().password 
            : rtu::RestClient::defaultAdminPasswords().front());

        getServerExtraInfo(base, password, localSuccessful, localFailed);
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

