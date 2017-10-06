#pragma once

#include <QString>

#include <nx/cloud/cdb/client/data/module_info.h>

#include "base_http_handler.h"
#include "../access_control/authentication_manager.h"
#include "../access_control/auth_types.h"
#include "../data/system_data.h"
#include "../managers/system_manager.h"
#include "../managers/managers_types.h"

namespace nx {
namespace cdb {
namespace http_handler {

class Ping:
    public FiniteMsgBodyHttpHandler<void, api::ModuleInfo>
{
public:
    static const QString kHandlerPath;

    Ping(const AuthorizationManager& authorizationManager):
        FiniteMsgBodyHttpHandler<void, api::ModuleInfo>(
            EntityType::module,
            DataActionType::fetch,
            authorizationManager,
            std::bind(
                &Ping::processPing, this,
                std::placeholders::_1, std::placeholders::_2))
    {
    }

private:
    void processPing(
        const AuthorizationInfo& /*authzInfo*/,
        std::function<void(api::ResultCode, api::ModuleInfo)> completionHandler)
    {
        api::ModuleInfo data;
        data.realm = AuthenticationManager::realm().constData();
        completionHandler(api::ResultCode::ok, std::move(data));
    }
};

} // namespace http_handler
} // namespace cdb
} // namespace nx
