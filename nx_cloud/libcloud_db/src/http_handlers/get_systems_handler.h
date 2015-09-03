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
    public AbstractFiniteMsgBodyHttpHandler<DataFilter, data::SystemDataList>
{
public:
    static const QString HANDLER_PATH;

    GetSystemsHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<DataFilter, data::SystemDataList>(
            EntityType::system,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &SystemManager::getSystems, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}
}

#endif  //NX_CLOUD_DB_GET_SYSTEMS_HANDLER_H
