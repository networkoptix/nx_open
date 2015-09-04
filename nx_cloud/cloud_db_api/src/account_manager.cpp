/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"

#include "generic_fixed_cdb_request.h"


namespace nx {
namespace cdb {
namespace cl {


void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    doRequest<api::AccountData, api::AccountData>(
        QUrl(),
        api::AccountData(),
        std::move(completionHandler));
}


}   //cl
}   //cdb
}   //nx
