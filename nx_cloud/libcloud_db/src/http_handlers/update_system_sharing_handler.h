/**********************************************************
* Oct 19, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_UPDATE_SYSTEM_SHARING_HANDLER_H
#define NX_CDB_UPDATE_SYSTEM_SHARING_HANDLER_H

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class SystemManager;

class UpdateSystemSharingHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemSharing>
{
public:
    static const QString HANDLER_PATH;

    UpdateSystemSharingHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemSharing>(
            EntityType::system,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &SystemManager::updateSharing, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //NX_CDB_UPDATE_SYSTEM_SHARING_HANDLER_H
