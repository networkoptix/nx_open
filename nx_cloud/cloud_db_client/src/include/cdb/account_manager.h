/**********************************************************
* Sep 3, 2015
* NetworkOptix
* akolesnikov
***********************************************************/

#ifndef NX_CDB_API_ACCOUNT_MANAGER_H
#define NX_CDB_API_ACCOUNT_MANAGER_H

#include <functional>

#include "account_data.h"
#include "result_code.h"


namespace nx {
namespace cdb {
namespace api {

class AccountManager
{
public:
    virtual ~AccountManager() {}

    /*!
        \param accountData. \a id and \a statusCode fields are ignored (assigned by server when creating account)
        \note Required access role: cloud module (e.g., user portal)
    */
    virtual void registerNewAccount(
        api::AccountData accountData,
        std::function<void(api::ResultCode, api::AccountActivationCode)> completionHandler) = 0;
    //!Activate account supplying code returned by \a AccountManager::registerNewAccount
    virtual void activateAccount(
        api::AccountActivationCode activationCode,
        std::function<void(api::ResultCode)> completionHandler) = 0;
    //!Fetches account info if credentails are account credentials
    /*!
        \note Required access role: account
    */
    virtual void getAccount(
        std::function<void(api::ResultCode, api::AccountData)> completionHandler) = 0;
};

}   //api
}   //cdb
}   //nx

#endif  //NX_CDB_API_ACCOUNT_MANAGER_H
