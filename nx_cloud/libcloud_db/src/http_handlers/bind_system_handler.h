/**********************************************************
* Aug 27, 2015
* a.kolesnikov
***********************************************************/

#ifndef BIND_SYSTEM_HANDLER_H
#define BIND_SYSTEM_HANDLER_H

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/email_verification_code.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class SystemManager;

class BindSystemHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemRegistrationData, data::SystemData>
{
public:
    static const QString HANDLER_PATH;

    BindSystemHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemRegistrationData, data::SystemData>(
            EntityType::system,
            DataActionType::insert,
            authorizationManager,
            std::bind(
                &SystemManager::bindSystemToAccount, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};


}   //cdb
}   //nx


#endif  //BIND_SYSTEM_HANDLER_H
