#include "setup_cloud_rest_handler.h"

#include <nx/network/http/http_types.h>

#include <nx/vms/cloud_integration/cloud_connection_manager.h>
#include <nx/vms/cloud_integration/vms_cloud_connection_processor.h>
#include <nx/vms/utils/vms_utils.h>

#include <api/app_server_connection.h>
#include <api/global_settings.h>
#include <api/model/password_data.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <nx_ec/data/api_conversion_functions.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

#include "save_cloud_system_credentials.h"
#include "system_settings_handler.h"

QnSetupCloudSystemRestHandler::QnSetupCloudSystemRestHandler(
    QnMediaServerModule* serverModule,
    nx::vms::cloud_integration::CloudManagerGroup* cloudManagerGroup)
    :
    nx::vms::server::ServerModuleAware(serverModule),
    m_cloudManagerGroup(cloudManagerGroup)
{
}

int QnSetupCloudSystemRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    return execute(std::move(SetupCloudSystemData(params)), owner, result);
}

int QnSetupCloudSystemRestHandler::executePost(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    const SetupCloudSystemData data = QJson::deserialized<SetupCloudSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnSetupCloudSystemRestHandler::execute(
    SetupCloudSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult& result)
{
    if (QnPermissionsHelper::isSafeMode(serverModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    nx::vms::cloud_integration::VmsCloudConnectionProcessor vmsCloudConnectionProcessor(
        owner->commonModule(),
        m_cloudManagerGroup);

    SystemSettingsProcessor systemSettingsProcessor(
        owner->commonModule(),
        owner->authSession());
    vmsCloudConnectionProcessor.setSystemSettingsProcessor(&systemSettingsProcessor);

    return vmsCloudConnectionProcessor.setupCloudSystem(
        owner->authSession(),
        data,
        &result);
}
