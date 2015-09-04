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
    doRequest(
        QUrl(),
        std::move(completionHandler));
}


}   //cl
}   //cdb
}   //nx
