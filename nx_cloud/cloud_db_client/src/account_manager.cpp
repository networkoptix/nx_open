/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"

#include "generic_fixed_cdb_request.h"


namespace nx {
namespace cdb {
namespace cl {


AccountManager::AccountManager(QUrl url)
:
    m_url(std::move(url))
{
}

void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    execute(
        getUrl()+"/get_account",
        std::move(completionHandler));
}

void AccountManager::setCredentials(
    const std::string& login,
    const std::string& password)
{
    QnMutexLocker lk(&m_mutex);
    m_url.setUsername(login);
    m_url.setPassword(password);
}

QUrl AccountManager::getUrl() const
{
    QnMutexLocker lk(&m_mutex);
    return m_url;
}


}   //cl
}   //cdb
}   //nx
