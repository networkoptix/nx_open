/**********************************************************
* Aug 19, 2015
* a.kolesnikov
***********************************************************/

#ifndef GET_ACCOUNT_HANDLER_H
#define GET_ACCOUNT_HANDLER_H

#include <QString>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/account_data.h"
#include "managers/account_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class GetAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<void, data::AccountData>
{
public:
    static const QString HANDLER_PATH;

    GetAccountHttpHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<void, data::AccountData>(
            EntityType::account,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &AccountManager::getAccount, accountManager,
                std::placeholders::_1, std::placeholders::_2 ) )
    {
    }
};

}
}

#endif  //GET_ACCOUNT_HANDLER_H
