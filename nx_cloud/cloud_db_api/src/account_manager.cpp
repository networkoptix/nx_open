/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#include "account_manager.h"


namespace nx {
namespace cdb {
namespace cl {


void AccountManager::getAccount(
    std::function<void(api::ResultCode, api::AccountData)> completionHandler)
{
    //TODO #ak
    completionHandler(api::ResultCode::notImplemented, api::AccountData());
}


}   //cl
}   //cdb
}   //nx
