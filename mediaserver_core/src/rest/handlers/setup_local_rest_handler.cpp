#include "setup_local_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>

#include <nx/vms/utils/setup_system_processor.h>
#include <nx/vms/utils/vms_utils.h>

#include <api/model/setup_local_system_data.h>
#include <api/resource_property_adaptor.h>
#include <core/resource_management/resource_pool.h>

#include "server/host_system_password_synchronizer.h"
#include "rest/server/rest_connection_processor.h"
#include "rest/helpers/permissions_helper.h"
#include "system_settings_handler.h"

int QnSetupLocalSystemRestHandler::executeGet(
    const QString& path,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    return execute(std::move(SetupLocalSystemData(params)), owner, result);
}

int QnSetupLocalSystemRestHandler::executePost(
    const QString& path,
    const QnRequestParams&,
    const QByteArray& body,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    SetupLocalSystemData data = QJson::deserialized<SetupLocalSystemData>(body);
    return execute(std::move(data), owner, result);
}

int QnSetupLocalSystemRestHandler::execute(
    SetupLocalSystemData data,
    const QnRestConnectionProcessor* owner,
    QnJsonRestResult& result)
{
    if (QnPermissionsHelper::isSafeMode(owner->commonModule()))
        return QnPermissionsHelper::safeModeError(result);
    if (!QnPermissionsHelper::hasOwnerPermissions(owner->resourcePool(), owner->accessRights()))
        return QnPermissionsHelper::notOwnerError(result);

    SystemSettingsProcessor systemSettingsProcessor(
        owner->commonModule(),
        owner->authSession());

    nx::vms::utils::SetupSystemProcessor setupSystemProcessor(owner->commonModule());
    setupSystemProcessor.setSystemSettingsProcessor(&systemSettingsProcessor);
    const auto resultCode = setupSystemProcessor.setupLocalSystem(
        owner->authSession(),
        data,
        &result);
    if (resultCode != nx::network::http::StatusCode::ok)
        return resultCode;

    if (auto admin = setupSystemProcessor.getModifiedLocalAdmin())
    {
        HostSystemPasswordSynchronizer::instance()->
            syncLocalHostRootPasswordWithAdminIfNeeded(admin);
    }

    return resultCode;
}
