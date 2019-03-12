#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/server/server_module_aware.h>

namespace ec2 {
    class AbstractTransactionMessageBus;
}

struct ConfigureSystemData;

class QnConfigureRestHandler:
    public QnJsonRestHandler,
    public /*mixin*/ nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    QnConfigureRestHandler(QnMediaServerModule* serverModule);

    virtual int executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
    virtual int executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) override;
private:
    int execute(const ConfigureSystemData& data, QnJsonRestResult &result, const QnRestConnectionProcessor* owner);
    int changePort(const QnRestConnectionProcessor* owner, int port);
};
