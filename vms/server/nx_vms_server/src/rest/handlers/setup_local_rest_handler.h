#pragma once

#include <nx/network/http/http_types.h>

#include <api/model/setup_local_system_data.h>
#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/server/server_module_aware.h>

class QnSetupLocalSystemRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnSetupLocalSystemRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(
        const QString &path,
        const QnRequestParams &params,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*owner) override;
    virtual int executePost(
        const QString &path,
        const QnRequestParams &params,
        const QByteArray &body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor*owner) override;
private:
    int execute(SetupLocalSystemData data, const QnRestConnectionProcessor* owner, QnJsonRestResult &result);
};
