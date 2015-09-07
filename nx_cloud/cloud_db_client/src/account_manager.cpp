/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"


namespace nx {
namespace cdb {
namespace cl {


AccountManager::AccountManager(QUrl url)
:
    m_url(std::move(url))
{
}

void AccountManager::registerNewAccount(
    const api::AccountData& accountData,
    std::function<void(api::ResultCode)> completionHandler)
{
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path() + "/account/register");
    execute(
        urlToExecute,
        accountData,
        std::move(completionHandler));
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    QUrl urlToExecute = url();
    urlToExecute.setPath(urlToExecute.path()+"/account/get");
    execute(
        urlToExecute,
        std::move(completionHandler));
}

void AccountManager::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_url.setUserName(QString::fromStdString(login));
    m_url.setPassword(QString::fromStdString(password));
}

QUrl AccountManager::url() const
{
    QnMutexLocker lk(&m_mutex);
    return m_url;
}


}   //cl
}   //cdb
}   //nx
