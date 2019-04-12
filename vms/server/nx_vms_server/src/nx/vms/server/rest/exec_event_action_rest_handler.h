#pragma once

#include "rest/server/json_rest_handler.h"
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server {

class QnExecuteEventActionRestHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    QnExecuteEventActionRestHandler(QnMediaServerModule* serverModule);

    virtual nx::network::rest::Response executePost(
        const nx::network::rest::Request& request) override;
};

} //namespace nx::vms::server
