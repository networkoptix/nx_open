#include "system_settings_processor.h"

#include <api/global_settings.h>
#include <api/model/system_settings_reply.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx_ec/access_helpers.h>
#include <rest/server/json_rest_result.h>

namespace nx {
namespace vms {
namespace utils {

SystemSettingsProcessor::SystemSettingsProcessor(QnCommonModule* commonModule):
    m_commonModule(commonModule)
{
}

void SystemSettingsProcessor::setOnBeforeUpdatingSettingValue(
    BeforeUpdatingSettingValueHandler handler)
{
    m_onBeforeUpdatingSettingValue = std::move(handler);
}

nx_http::StatusCode::Value SystemSettingsProcessor::updateSettings(
    const Qn::UserAccessData& accessRights,
    const QnAuthSession& authSession,
    const QnRequestParams& params,
    QnJsonRestResult* result)
{
    QnSystemSettingsReply reply;

    bool dirty = false;
    const auto& settings = m_commonModule->globalSettings()->allSettings();

    QnRequestParams filteredParams(params);
    filteredParams.remove(lit("auth"));

    namespace ahlp = ec2::access_helpers;

    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        bool readAllowed = ahlp::kvSystemOnlyFilter(
            ahlp::Mode::read,
            accessRights,
            setting->key());

        bool writeAllowed = ec2::access_helpers::kvSystemOnlyFilter(
            ahlp::Mode::write,
            accessRights,
            setting->key());

        writeAllowed &= m_commonModule->resourceAccessManager()->hasGlobalPermission(
            accessRights,
            Qn::GlobalPermission::GlobalAdminPermission);

        if (!filteredParams.isEmpty())
        {
            auto paramIter = filteredParams.find(setting->key());
            if (paramIter == filteredParams.end())
                continue;

            if (!writeAllowed)
                return nx_http::StatusCode::forbidden;

            m_onBeforeUpdatingSettingValue(
                setting->key(),
                setting->serializedValue(),
                paramIter.value());

            setting->setSerializedValue(paramIter.value());
            dirty = true;
        }

        if (readAllowed)
            reply.settings.insert(setting->key(), setting->serializedValue());
    }
    if (dirty)
        m_commonModule->globalSettings()->synchronizeNow();

    result->setReply(std::move(reply));

    return nx_http::StatusCode::ok;
}

} // namespace utils
} // namespace vms
} // namespace nx
