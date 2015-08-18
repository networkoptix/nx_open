/**********************************************************
* Aug 18, 2015
* a.kolesnikov
***********************************************************/

#ifndef VERIFY_EMAIL_ADDRESS_HANDLER_H
#define VERIFY_EMAIL_ADDRESS_HANDLER_H

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/email_verification_code.h"
#include "managers/account_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class AccountManager;

class VerifyEmailAddressHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::EmailVerificationCode>
{
public:
    static const QString HANDLER_PATH;

    VerifyEmailAddressHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::EmailVerificationCode>(
            EntityType::account,
            DataActionType::update,
            authorizationManager,
            std::bind( 
                &AccountManager::verifyAccountEmailAddress, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};


}   //cdb
}   //nx

#endif  //VERIFY_EMAIL_ADDRESS_HANDLER_H
