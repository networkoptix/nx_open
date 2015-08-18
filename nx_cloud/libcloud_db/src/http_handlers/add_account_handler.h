/**********************************************************
* 16 may 2015
* a.kolesnikov
***********************************************************/

#ifndef cloud_db_add_account_handler_h
#define cloud_db_add_account_handler_h

#include <memory>

#include <QString>

#include "access_control/types.h"
#include "base_http_handler.h"
#include "data/account_data.h"
#include "managers/account_manager.h"
#include "managers/types.h"


namespace nx {
namespace cdb {


class AccountManager;

class AddAccountHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::AccountData>
{
public:
    static const QString HANDLER_PATH;

    AddAccountHttpHandler(
        AccountManager* const accountManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::AccountData>(
            EntityType::account,
            DataActionType::insert,
            authorizationManager,
            std::bind(
                &AccountManager::addAccount, accountManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};


}   //cdb
}   //nx

#endif  //cloud_db_add_account_handler_h
