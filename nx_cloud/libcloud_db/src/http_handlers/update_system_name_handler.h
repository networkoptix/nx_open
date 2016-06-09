/**********************************************************
* Jun 8, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <memory>

#include <QString>

#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/system_data.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {

class UpdateSystemNameHttpHandler
:
    public AbstractFiniteMsgBodyHttpHandler<data::SystemNameUpdate>
{
public:
    static const QString kHandlerPath;

    UpdateSystemNameHttpHandler(
        SystemManager* const systemManager,
        const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<data::SystemNameUpdate>(
            EntityType::system,
            DataActionType::update,
            authorizationManager,
            std::bind(
                &SystemManager::updateSystemName, systemManager,
                std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
    {
    }
};

}   //cdb
}   //nx
