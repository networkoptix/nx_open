/**********************************************************
* Sep 3, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_ACCOUNT_MANAGER_H
#define NX_CDB_API_ACCOUNT_MANAGER_H

#include <functional>

#include "account_data.h"
#include "async_http_requests_executor.h"
#include "result_code.h"


namespace nx {
namespace cdb {
namespace api {

class AccountManager
{
public:
    virtual ~AccountManager() {}

    //!Fetches account info if credentails are account credentials
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) = 0;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_ACCOUNT_MANAGER_H
