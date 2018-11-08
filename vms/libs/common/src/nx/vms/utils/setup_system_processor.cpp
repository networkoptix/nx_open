#include "setup_system_processor.h"

#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>

#include <api/global_settings.h>
#include <api/model/setup_local_system_data.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <rest/server/json_rest_result.h>

#include "system_settings_processor.h"
#include "vms_utils.h"

namespace nx {
namespace vms {
namespace utils {

SetupSystemProcessor::SetupSystemProcessor(QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
}

void SetupSystemProcessor::setSystemSettingsProcessor(
    nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessor)
{
    m_systemSettingsProcessor = systemSettingsProcessor;
}

nx::network::http::StatusCode::Value SetupSystemProcessor::setupLocalSystem(
    const QnAuthSession& authSession,
    const SetupLocalSystemData& data,
    QnJsonRestResult* result)
{
    if (!m_commonModule->globalSettings()->isNewSystem())
    {
        result->setError(
            QnJsonRestResult::Forbidden,
            lit("This method is allowed at initial state only. Use 'api/detachFromSystem' method first."));
        return nx::network::http::StatusCode::ok;
    }

    QString errStr;
    if (!nx::vms::utils::validatePasswordData(data, &errStr))
    {
        result->setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx::network::http::StatusCode::ok;
    }
    if (!data.hasPassword())
    {
        result->setError(
            QnJsonRestResult::MissingParameter,
            lit("Password or password digest MUST be provided"));
        return nx::network::http::StatusCode::ok;
    }

    if (data.systemName.isEmpty())
    {
        result->setError(
            QnJsonRestResult::MissingParameter
            , lit("Parameter 'systemName' must be provided."));
        return nx::network::http::StatusCode::ok;
    }

    const auto systemNameBak = m_commonModule->globalSettings()->systemName();

    m_commonModule->globalSettings()->resetCloudParams();
    m_commonModule->globalSettings()->setSystemName(data.systemName);
    m_commonModule->globalSettings()->setLocalSystemId(QnUuid::createUuid());

    if (!m_commonModule->globalSettings()->synchronizeNowSync())
    {
        // Changing system name back.
        m_commonModule->globalSettings()->setSystemName(systemNameBak);
        m_commonModule->globalSettings()->setLocalSystemId(QnUuid()); //< revert
        result->setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Internal server error."));
        return nx::network::http::StatusCode::ok;
    }

    if (!nx::vms::utils::updateUserCredentials(
            m_commonModule->ec2Connection(),
            data,
            QnOptionalBool(true),
            m_commonModule->resourcePool()->getAdministrator(),
            &errStr,
            &m_modifiedAdminUser))
    {
        // Changing system name back.
        m_commonModule->globalSettings()->setSystemName(systemNameBak);
        result->setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx::network::http::StatusCode::ok;
    }

    std::unique_ptr<nx::vms::utils::SystemSettingsProcessor> defaultSystemSettingsProcessor;
    nx::vms::utils::SystemSettingsProcessor* systemSettingsProcessorToUse = nullptr;
    if (m_systemSettingsProcessor)
    {
        systemSettingsProcessorToUse = m_systemSettingsProcessor;
    }
    else
    {
        defaultSystemSettingsProcessor =
            std::make_unique<nx::vms::utils::SystemSettingsProcessor>(m_commonModule);
        systemSettingsProcessorToUse = defaultSystemSettingsProcessor.get();
    }

    const auto resultCode = systemSettingsProcessorToUse->updateSettings(
        Qn::kSystemAccess,
        authSession,
        data.systemSettings,
        result);
    if (resultCode != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(this, lm("Failed to write system settings. %1")
            .arg(nx::network::http::StatusCode::toString(resultCode)));
    }

    return nx::network::http::StatusCode::ok;
}

QnUserResourcePtr SetupSystemProcessor::getModifiedLocalAdmin() const
{
    return m_modifiedAdminUser;
}

} // namespace utils
} // namespace vms
} // namespace nx
