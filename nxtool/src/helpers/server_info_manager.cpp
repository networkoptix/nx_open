
#include "server_info_manager.h"

#include <functional>

#include <QStringList>
#include <QElapsedTimer>

#include <base/server_info.h>
#include <base/requests.h>
#include <helpers/http_client.h>

namespace
{
    QStringList g_availablePasswords = rtu::defaultAdminPasswords();

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
    
    void primaryLoginToServer(const BaseServerInfo &info
        , const QString &password
        , const ServerInfoManager::SuccessfulCallback &successful
        , const ServerInfoManager::FailedCallback &failed);
    
    void loginToServer(const BaseServerInfo &info
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
    HttpClient * const m_httpClient;

    QElapsedTimer m_msCounter;
    LastUpdateTimeMap m_lastUpdated;
};

rtu::ServerInfoManager::Impl::Impl(ServerInfoManager *parent)
    : QObject(parent)
    , m_owner(parent)
    , m_httpClient(new HttpClient(this))

    , m_msCounter()
    , m_lastUpdated()
{   
}

rtu::ServerInfoManager::Impl::~Impl()
{
}

void rtu::ServerInfoManager::Impl::primaryLoginToServer(const BaseServerInfo &info
    , const QString &password
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info.hostAddress);
    };

    const auto &localFailed = [this, info, failed](const QString &, int)
    {
        if (failed)
            failed(info.id);
    };
    
    enum { kShortTimeout = 2000};
    getServerExtraInfo(m_httpClient, info, password, localSuccessful, localFailed, kShortTimeout);
} 

void rtu::ServerInfoManager::Impl::loginToServer(const BaseServerInfo &info
    , int passwordIndex
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{
    if (passwordIndex >= g_availablePasswords.size())
    {
        if (failed)
            failed(info.id);
        return;
    }
    
    const auto &localFailed = 
        [this, info, passwordIndex, successful, failed](const QString &,int)
    {
        loginToServer(info, passwordIndex + 1, successful, failed);
    };
    
    const auto &localSuccessful = [successful, info](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra, info.hostAddress);
    };

    getServerExtraInfo(m_httpClient, info
        , g_availablePasswords.at(passwordIndex), localSuccessful, localFailed);
}

void rtu::ServerInfoManager::Impl::updateServerInfos(const ServerInfoContainer &servers
    , const ServerInfoManager::SuccessfulCallback &successful
    , const ServerInfoManager::FailedCallback &failed)
{

    for (const ServerInfo &server: servers)
    {
        const BaseServerInfo &base = server.baseInfo();
        const int timestamp = m_msCounter.elapsed();
        const auto &localSuccessful = 
            [this, base, timestamp, successful](const QUuid &, const ExtraServerInfo &extraInfo)
        {
            m_lastUpdated[base.id] = timestamp;
            if (successful)
                successful(base.id, extraInfo, base.hostAddress);
        };

        const auto &localFailed = 
            [this, base, successful, timestamp, failed](const QString &, int) 
        {
            /// on fail - try re-login
            const auto &loginFailed = [this, timestamp, failed](const QUuid &id)
            {
                if (m_lastUpdated[id] <= timestamp)
                    failed(id);
            };

            loginToServer(base, 0, successful, loginFailed);
        };

        const QString password = (server.hasExtraInfo() ? server.extraInfo().password 
            : defaultAdminPasswords().front());
        getServerExtraInfo(m_httpClient, server.baseInfo()
            , password, localSuccessful, localFailed);
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
    m_impl->loginToServer(info, 0, success, failed);
}

void rtu::ServerInfoManager::loginToServer(const BaseServerInfo &info
    , const QString &password
    , const SuccessfulCallback &success
    , const FailedCallback &failed)
{
    m_impl->primaryLoginToServer(info, password, success, failed);
}

void rtu::ServerInfoManager::updateServerInfos(const ServerInfoContainer &servers
    , const SuccessfulCallback &success
    , const FailedCallback &failed)
{
    m_impl->updateServerInfos(servers, success, failed);
}

