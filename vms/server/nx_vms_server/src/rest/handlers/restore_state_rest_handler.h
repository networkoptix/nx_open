#pragma once

#include <rest/server/json_rest_handler.h>
#include <core/resource_access/user_access_data.h>
#include <nx/vms/server/server_module_aware.h>

struct PasswordData;

/**
 * Restore media server state to default:
 * 1. Reset system name
 * 2. Restore database state
 */

class QnRestoreStateRestHandler:
    public QnJsonRestHandler,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT
public:
    // virtual int executeGet for RestoreState request must not be overriden.
    // Parent's method returns badRequest for this request.

    QnRestoreStateRestHandler(QnMediaServerModule* serverModule);

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult &result,
        const QnRestConnectionProcessor* owner) override;

private:
    virtual void afterExecute(
        const QString& path,
        const QnRequestParamList& params,
        const QByteArray& body,
        const QnRestConnectionProcessor* owner) override;
};
