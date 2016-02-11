/**********************************************************
* Oct 9, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CDB_GET_CLOUD_USERS_OF_SYSTEM_H
#define NX_CDB_GET_CLOUD_USERS_OF_SYSTEM_H

#include <cloud_db_client/src/data/auth_data.h>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class GetCloudUsersOfSystemHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::DataFilter, api::SystemSharingExList>
{
public:
    static const QString kHandlerPath;

    GetCloudUsersOfSystemHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::DataFilter, api::SystemSharingExList>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &SystemManager::getCloudUsersOfSystem, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //NX_CDB_GET_CLOUD_USERS_OF_SYSTEM_H
