/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_ACTIVATE_ACCOUNT_HANDLER_H
#define NX_CDB_ACTIVATE_ACCOUNT_HANDLER_H

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "managers/account_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class ActivateAccountHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountConfirmationCode, api::AccountEmail>
{
public:
    static const QString kHandlerPath;

    ActivateAccountHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountConfirmationCode, api::AccountEmail>(
            EntityType::account,
            DataActionType::update,
            authorizationManager,
            std::bind( 
                &AccountManager::activate, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};

}   //cdb
}   //nx

#endif  //NX_CDB_ACTIVATE_ACCOUNT_HANDLER_H
