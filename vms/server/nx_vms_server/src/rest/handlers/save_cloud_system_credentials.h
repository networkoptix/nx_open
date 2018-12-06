#pragma once

#include <core/resource_access/user_access_data.h>
#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

struct CloudCredentialsData;
namespace nx { namespace vms { namespace cloud_integration { class CloudManagerGroup; } } }
class QnCommonModule;

class QnSaveCloudSystemCredentialsHandler:
    public QnJsonRestHandler,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnSaveCloudSystemCredentialsHandler(
        QnMediaServerModule* serverModule,
        nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup);

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor*);

    int execute(
        const CloudCredentialsData& data,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner);

private:
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup = nullptr;

    bool authorize(
        const Qn::UserAccessData& accessRights,
        QnJsonRestResult* result,
        nx::network::http::StatusCode::Value* const authorizationStatusCode);
};
