/**********************************************************
* Sep 1, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_UNBIND_SYSTEM_HANDLER_H
#define NX_CLOUD_DB_UNBIND_SYSTEM_HANDLER_H

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/email_verification_code.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class SystemManager;

class UnbindSystemHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemID>
{
public:
    static const QString HANDLER_PATH;

    UnbindSystemHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemID>(
            EntityType::system,
            DataActionType::_delete,
            authorizationManager,
            std::bind(
                &SystemManager::unbindSystem, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};


}   //cdb
}   //nx


#endif  //NX_CLOUD_DB_UNBIND_SYSTEM_HANDLER_H
