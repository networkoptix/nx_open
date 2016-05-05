/**********************************************************
* Sep 28, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_SHARE_SYSTEM_HANDLER_H
#define NX_CDB_SHARE_SYSTEM_HANDLER_H

#include <memory>

#include <QString>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/system_data.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class SystemManager;

class ShareSystemHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemSharing>
{
public:
    static const QString kHandlerPath;

    ShareSystemHttpHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager )
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemSharing>(
            EntityType::system,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &SystemManager::shareSystem, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3 ) )
    {
    }
};


}   //cdb
}   //nx

#endif  //NX_CDB_SHARE_SYSTEM_HANDLER_H
