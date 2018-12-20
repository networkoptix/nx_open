#pragma once

#include <core/resource/resource_fwd.h>
#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnExternalEventRestHandler:
    public QnJsonRestHandler, public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnExternalEventRestHandler(QnMediaServerModule* serverModule);

    QnExternalEventRestHandler();

    virtual int executeGet(const QString& path, const QnRequestParams& params,
        QnJsonRestResult& result, const QnRestConnectionProcessor* owner) override;
};
