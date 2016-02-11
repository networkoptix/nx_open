/**********************************************************
* dec 9, 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_reset_password_handler_h
#define cloud_db_reset_password_handler_h

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

class ResetPasswordHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountEmail, data::AccountConfirmationCode>
{
public:
    static const QString kHandlerPath;

    ResetPasswordHttpHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountEmail, data::AccountConfirmationCode>(
            EntityType::account,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &AccountManager::resetPassword, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //cloud_db_reset_password_handler_h
