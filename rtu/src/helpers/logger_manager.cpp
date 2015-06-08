
#include "login_manager.h"

#include <QStringList>

#include <server_info.h>
#include <requests/requests.h>
#include <helpers/http_client.h>

namespace
{
    QStringList g_availablePasswords({rtu::defaultAdminPassword()});
}

class rtu::LoginManager::Impl : public QObject
{
public:
    Impl(LoginManager *parent);
    
    virtual ~Impl();
    
    void primaryLoginToServer(const BaseServerInfo &info
        , const QString &password);
    
    void loginToServer(const BaseServerInfo &info
        , int passwordIndex);
    
private:
    LoginManager * const m_owner;
    HttpClient * const m_httpClient;
};

rtu::LoginManager::Impl::Impl(LoginManager *parent)
    : QObject(parent)
    , m_owner(parent)
    , m_httpClient(new HttpClient(this))
{   
}

rtu::LoginManager::Impl::~Impl()
{
}

void rtu::LoginManager::Impl::primaryLoginToServer(const BaseServerInfo &info
    , const QString &password)
{
    const auto & successful = [this](const QUuid &id
        , const rtu::ExtraServerInfo &extraInfo) 
    {
        emit m_owner->loginOperationSuccessfull(id, extraInfo);
    };
    
    const auto &failed = [this, info](const QUuid &id)
    {
        loginToServer(info, 0);
    };
    
    getServerExtraInfo(m_httpClient, info, password, successful, failed);
} 

void rtu::LoginManager::Impl::loginToServer(const BaseServerInfo &info
    , int passwordIndex)
{
    if (passwordIndex >= g_availablePasswords.size())
    {
        emit m_owner->loginOperationFailed(info.id);
        return;
    }

    const auto & successful = [this](const QUuid &id
        , const rtu::ExtraServerInfo &extraInfo) 
    {
        emit m_owner->loginOperationSuccessfull(id, extraInfo);
    };
    
    const auto &failed = [this, info, passwordIndex](const QUuid &id)
    {
        loginToServer(info, passwordIndex + 1);
    };
    
    getServerExtraInfo(m_httpClient, info
        , g_availablePasswords.at(passwordIndex), successful, failed);
}

///

rtu::LoginManager::LoginManager(QObject *parent)
    : QObject(parent)
    , m_impl(new Impl(this))
{
    
}

rtu::LoginManager::~LoginManager()
{   
}

void rtu::LoginManager::loginToServer(const BaseServerInfo &info)
{
    m_impl->loginToServer(info, 0);
}

void rtu::LoginManager::loginToServer(const BaseServerInfo &info
    , const QString &password)
{
    m_impl->primaryLoginToServer(info, password);
}
