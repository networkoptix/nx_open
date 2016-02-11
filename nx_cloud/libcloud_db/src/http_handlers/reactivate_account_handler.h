/**********************************************************
* Dec 16, 2015
* akolesnikov
***********************************************************/

#ifndef CLOUD_DB_REACTIVATE_ACCOUNT_HANDLER_H
#define CLOUD_DB_REACTIVATE_ACCOUNT_HANDLER_H

#include <memory>

#include <QString>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/account_data.h"
#include "managers/account_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class AccountManager;

class ReactivateAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountEmail, data::AccountConfirmationCode>
{
public:
    static const QString kHandlerPath;

    ReactivateAccountHttpHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountEmail, data::AccountConfirmationCode>(
            EntityType::account,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &AccountManager::reactivateAccount, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //CLOUD_DB_REACTIVATE_ACCOUNT_HANDLER_H
