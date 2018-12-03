#pragma once

#include <api/model/setup_cloud_system_data.h>
#include <core/resource_access/user_access_data.h>
#include <rest/server/json_rest_handler.h>
#include <nx/vms/server/server_module_aware.h>

namespace nx { namespace vms { namespace cloud_integration { class CloudManagerGroup; } } }

class QnSetupCloudSystemRestHandler:
    public QnJsonRestHandler,
    public nx::vms::server::ServerModuleAware
{
    Q_OBJECT

public:
    QnSetupCloudSystemRestHandler(
        QnMediaServerModule* serverModule,
        nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup);

    virtual int executeGet(
        const QString& path,
        const QnRequestParams& params,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

    virtual int executePost(
        const QString& path,
        const QnRequestParams& params,
        const QByteArray& body,
        QnJsonRestResult& result,
        const QnRestConnectionProcessor* owner) override;

private:
    nx::vms::cloud_integration::CloudManagerGroup* m_cloudManagerGroup;

    int execute(
        SetupCloudSystemData data,
        const QnRestConnectionProcessor* owner,
        QnJsonRestResult& result);
};
