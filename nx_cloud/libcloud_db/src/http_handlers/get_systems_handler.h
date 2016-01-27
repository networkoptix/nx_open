/**********************************************************
* Sep 1, 2015
* a.kolesnikov
***********************************************************/

#ifndef NX_CLOUD_DB_GET_SYSTEMS_HANDLER_H
#define NX_CLOUD_DB_GET_SYSTEMS_HANDLER_H

#include <QString>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/system_data.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class GetSystemsHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::DataFilter, api::SystemDataExList>
{
public:
    static const QString kHandlerPath;

    GetSystemsHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::DataFilter, api::SystemDataExList>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &SystemManager::getSystems, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx

#endif  //NX_CLOUD_DB_GET_SYSTEMS_HANDLER_H
