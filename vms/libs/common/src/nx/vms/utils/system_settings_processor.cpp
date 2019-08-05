#include "system_settings_processor.h"

#include <api/global_settings.h>
#include <api/model/system_settings_reply.h>
#include <api/resource_property_adaptor.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <nx_ec/access_helpers.h>
#include <rest/server/json_rest_result.h>
#include <audit/audit_manager.h>

#include <QStringLiteral>

namespace nx {
namespace vms {
namespace utils {

namespace {

static const auto kIgnoreKey = QStringLiteral("ignore");

static QSet<QString> ignoredKeysFromParams(const QnRequestParams& params)
{
    if (!params.contains(kIgnoreKey))
        return QSet<QString>();

    return params[kIgnoreKey].trimmed().split(",", QString::SplitBehavior::SkipEmptyParts).toSet();
}

} // namespace

SystemSettingsProcessor::SystemSettingsProcessor(QnCommonModule* commonModule):
    QnCommonModuleAware(commonModule)
{
}

void SystemSettingsProcessor::setOnBeforeUpdatingSettingValue(
    BeforeUpdatingSettingValueHandler handler)
{
    m_onBeforeUpdatingSettingValue = std::move(handler);
}

nx::network::http::StatusCode::Value SystemSettingsProcessor::updateSettings(
    const Qn::UserAccessData& accessRights,
    const QnAuthSession& authSession,
    const QnRequestParams& params,
    QnJsonRestResult* result)
{
    QnSystemSettingsReply reply;

    bool dirty = false;
    const auto& settings = commonModule()->globalSettings()->allSettings();

    QnRequestParams filteredParams(params);
    filteredParams.remove(lit("auth"));
    const auto ignoredKeys = ignoredKeysFromParams(filteredParams);
    filteredParams.remove(kIgnoreKey);

    namespace ahlp = ec2::access_helpers;

    for (QnAbstractResourcePropertyAdaptor* setting: settings)
    {
        bool writeAllowed = ec2::access_helpers::kvSystemOnlyFilter(
            ahlp::Mode::write,
            accessRights,
            setting->key());

        writeAllowed &= commonModule()->resourceAccessManager()->hasGlobalPermission(
            accessRights,
            GlobalPermission::admin);

        if (!filteredParams.isEmpty())
        {
            auto paramIter = filteredParams.find(setting->key());
            if (paramIter == filteredParams.end())
                continue;

            if (!writeAllowed)
                return nx::network::http::StatusCode::forbidden;

            m_onBeforeUpdatingSettingValue(
                setting->key(),
                setting->serializedValue(),
                paramIter.value());

            setting->setSerializedValue(paramIter.value());
            dirty = true;
            auditManager()->notifySettingsChanged(authSession, setting->key());
        }

        const bool readAllowed =
            ahlp::kvSystemOnlyFilter(ahlp::Mode::read, accessRights, setting->key());

        if (readAllowed && !ignoredKeys.contains(setting->key()))
            reply.settings.insert(setting->key(), setting->serializedValue());
    }

    if (dirty)
        commonModule()->globalSettings()->synchronizeNow();

    result->setReply(std::move(reply));
    return nx::network::http::StatusCode::ok;
}

} // namespace utils
} // namespace vms
} // namespace nx
