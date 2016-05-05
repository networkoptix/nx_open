/**********************************************************
* Jan 26, 2016
* a.kolesnikov
***********************************************************/

#pragma once

#include <cloud_db_client/src/data/auth_data.h>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class GetAccessRoleListHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemID, api::SystemAccessRoleList>
{
public:
    static const QString kHandlerPath;

    GetAccessRoleListHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemID, api::SystemAccessRoleList>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &SystemManager::getAccessRoleList, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx
