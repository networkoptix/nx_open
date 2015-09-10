/**********************************************************
* Sep 10, 2015
* akolesnikov
***********************************************************/

#ifndef NX_CDB_PING_H
#define NX_CDB_PING_H

#include <QString>

#include <cloud_db_client/src/data/module_info.h>

#include "access_control/authentication_manager.h"
#include "access_control/auth_types.h"
#include "base_http_handler.h"
#include "data/system_data.h"
#include "managers/system_manager.h"
#include "managers/managers_types.h"


namespace nx {
namespace cdb {


class PingHandler
:
    public AbstractFiniteMsgBodyHttpHandler<void, api::ModuleInfo>
{
public:
    static const QString HANDLER_PATH;

    PingHandler(const AuthorizationManager& authorizationManager)
    :
        AbstractFiniteMsgBodyHttpHandler<void, api::ModuleInfo>(
            EntityType::module,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &PingHandler::processPing, this,
                std::placeholders::_1, std::placeholders::_2))
    {
    }

private:
    void processPing(
        const AuthorizationInfo& /*authzInfo*/,
        std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
    {
        api::ModuleInfo data;
        data.realm = AuthenticationManager::realm();
        completionHandler(api::ResultCode::ok, std::move(data));
    }
};

}   //cdb
}   //nx

#endif  //NX_CDB_PING_H
