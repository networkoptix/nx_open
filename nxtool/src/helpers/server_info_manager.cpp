
#include "server_info_manager.h"

#include <functional>

#include <QStringList>

#include <base/server_info.h>
#include <base/requests.h>
#include <helpers/http_client.h>

namespace
{
    QStringList g_availablePasswords = rtu::defaultAdminPasswords();

    typedef std::function<void (const QUuid &id)> LoginFailedCallback;
    typedef std::function<void (const QUuid &id
        , const rtu::ExtraServerInfo &extraInfo)> LoginSuccessfulCallback;

    const auto globalSuccessful = [](const LoginSuccessfulCallback &callback
        , const QUuid &id, const rtu::ExtraServerInfo &extraInfo)
    {
        if (!g_availablePasswords.contains(extraInfo.password))
            g_availablePasswords.push_back(extraInfo.password);

        if (callback)
        {
            callback(id, extraInfo);
        }
    };
}

class rtu::ServerInfoManager::Impl : public QObject
{
public:
    Impl(ServerInfoManager *parent);
    
    virtual ~Impl();
    
    void primaryLoginToServer(const BaseServerInfo &info
        , const QString &password
        , const LoginSuccessfulCallback &successful
        , const LoginFailedCallback &failed);
    
    void loginToServer(const BaseServerInfo &info
        , int passwordIndex
        , const LoginSuccessfulCallback &successful
        , const LoginFailedCallback &failed);
    
    ///

    void updateServerInfos(const ServerInfoContainer &servers);

private:
    ServerInfoManager * const m_owner;
    HttpClient * const m_httpClient;
};

rtu::ServerInfoManager::Impl::Impl(ServerInfoManager *parent)
    : QObject(parent)
    , m_owner(parent)
    , m_httpClient(new HttpClient(this))
{   
}

rtu::ServerInfoManager::Impl::~Impl()
{
}

void rtu::ServerInfoManager::Impl::primaryLoginToServer(const BaseServerInfo &info
    , const QString &password
    , const LoginSuccessfulCallback &successful
    , const LoginFailedCallback &failed)
{
    const auto &localFailed = [this, info, successful, failed](const QString &, int)
    {
        loginToServer(info, 0, successful, failed);
    };
    
    const auto &localSuccessful = [successful](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra);
    };

    getServerExtraInfo(m_httpClient, info, password, localSuccessful, localFailed);
} 

void rtu::ServerInfoManager::Impl::loginToServer(const BaseServerInfo &info
    , int passwordIndex
    , const LoginSuccessfulCallback &successful
    , const LoginFailedCallback &failed)
{
    if (passwordIndex >= g_availablePasswords.size())
    {
        failed(info.id);
        return;
    }
    
    const auto &localFailed = 
        [this, info, passwordIndex, successful, failed](const QString &,int)
    {
        loginToServer(info, passwordIndex + 1, successful, failed);
    };
    
    const auto &localSuccessful = [successful](const QUuid &id, const ExtraServerInfo &extra)
    {
        globalSuccessful(successful, id, extra);
    };

    getServerExtraInfo(m_httpClient, info
        , g_availablePasswords.at(passwordIndex), localSuccessful, localFailed);
}

void rtu::ServerInfoManager::Impl::updateServerInfos(const ServerInfoContainer &servers)
{

    for (const ServerInfo &server: servers)
    {
        const BaseServerInfo &base = server.baseInfo();
        const QUuid &id = base.id;
        const auto &successful = [this, id](const QUuid &, const ExtraServerInfo &extraInfo)
        {
            emit m_owner->serverExtraInfoUpdated(id, extraInfo);
        };

        const auto &failed = [this, base, successful](const QString &, int) 
        {
            /// on fail - try re-login
            const auto &loginFailed = [this](const QUuid &id)
            {
                emit m_owner->serverExtraInfoUpdateFailed(id);
            };

            loginToServer(base, 0, successful, loginFailed);
        };

        const QString password = (server.hasExtraInfo() ? server.extraInfo().password 
            : defaultAdminPasswords().front());
        getServerExtraInfo(m_httpClient, server.baseInfo()
            , password, successful, failed);
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

void rtu::ServerInfoManager::loginToServer(const BaseServerInfo &info)
{
    const auto &successful = [this](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        emit loginOperationSuccessfull(id, extraInfo);
    };

    const auto &failed = [this](const QUuid &id)
    {
        emit loginOperationFailed(id);
    };

    m_impl->loginToServer(info, 0, successful, failed);
}

void rtu::ServerInfoManager::loginToServer(const BaseServerInfo &info
    , const QString &password)
{
    const auto &successful = [this](const QUuid &id, const ExtraServerInfo &extraInfo)
    {
        emit loginOperationSuccessfull(id, extraInfo);
    };

    const auto &failed = [this](const QUuid &id)
    {
        emit loginOperationFailed(id);
    };

    m_impl->primaryLoginToServer(info, password, successful, failed);
}

void rtu::ServerInfoManager::updateServerInfos(const ServerInfoContainer &servers)
{
    m_impl->updateServerInfos(servers);
}

