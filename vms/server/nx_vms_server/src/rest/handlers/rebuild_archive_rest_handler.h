#pragma once

#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

class QnRebuildArchiveRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnRebuildArchiveRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*) override;
};
