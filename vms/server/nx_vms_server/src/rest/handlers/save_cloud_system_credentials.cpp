#include "save_cloud_system_credentials.h"

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>
#include <nx/utils/sync_call.h>

#include <nx/cloud/cdb/api/connection.h>
#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/cloud_manager_group.h>
#include <nx/vms/cloud_integration/vms_cloud_connection_processor.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/cloud_credentials_data.h>
#include <common/common_module.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

QnSaveCloudSystemCredentialsHandler::QnSaveCloudSystemCredentialsHandler(
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
:
    nx::mediaserver::ServerModuleAware(serverModule),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

int QnSaveCloudSystemCredentialsHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const CloudCredentialsData data = QJson::deserialized<CloudCredentialsData>(body);
    return execute(data, result, owner);
}

int QnSaveCloudSystemCredentialsHandler::execute(
    const CloudCredentialsData& data,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    using namespace nx::cdb;

    nx::network::http::StatusCode::Value statusCode = nx::network::http::StatusCode::ok;
    if (!authorize(owner->accessRights(), &result, &statusCode))
        return statusCode;

    nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
        owner->commonModule(),
        m_cloudManagerGroup);

    return vmsCloudConnectionProcessor.bindSystemToCloud(data, &result);
}

bool QnSaveCloudSystemCredentialsHandler::authorize(
    const Qn::UserAccessData& accessRights,
    QnJsonRestResult* result,
    nx::network::http::StatusCode::Value* const authorizationStatusCode)
{
    using namespace nx::network::http;

    if (QnPermissionsHelper::isSafeMode(serverModule()))
    {
        *authorizationStatusCode = (StatusCode::Value)QnPermissionsHelper::safeModeError(*result);
        return false;
    }
    if (!QnPermissionsHelper::hasOwnerPermissions(resourcePool(), accessRights))
    {
        *authorizationStatusCode = (StatusCode::Value)QnPermissionsHelper::notOwnerError(*result);
        return false;
    }

    return true;
}
