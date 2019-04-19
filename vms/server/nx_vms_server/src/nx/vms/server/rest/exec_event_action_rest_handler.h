#pragma once

#include "rest/server/json_rest_handler.h"
#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class QnExecuteEventActionRestHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
    QnExecuteEventActionRestHandler(QnMediaServerModule* serverModule);

    virtual JsonRestResponse executePost(
        const JsonRestRequest& request, const QByteArray& body) override;
};

} //namespace nx::vms::server::rest