/**********************************************************
* 25 oct 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_update_account_handler_h
#define cloud_db_update_account_handler_h

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

class UpdateAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountUpdateData>
{
public:
    static const QString kHandlerPath;

    UpdateAccountHttpHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountUpdateData>(
            EntityType::account,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &AccountManager::updateAccount, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //cloud_db_update_account_handler_h
