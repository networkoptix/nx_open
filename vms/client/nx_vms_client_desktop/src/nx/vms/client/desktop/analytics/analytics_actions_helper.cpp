// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_actions_helper.h"

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/ui/dialogs/analytics_action_settings_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

void AnalyticsActionsHelper::processResult(
    const AnalyticsActionResult& result,
    QnWorkbenchContext* context,
    const QnResourcePtr& proxyResource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    QWidget* parent)
{
    if (!result.messageToUser.isEmpty())
    {
        QnMessageBox message(
            QnMessageBox::Icon::Success,
            result.messageToUser,
            /*extras*/ "",
            QDialogButtonBox::Ok,
            QDialogButtonBox::NoButton,
            parent);

        setHelpTopic(&message, HelpTopic::Id::Forced_Empty);
        message.exec();
    }

    if (!result.actionUrl.isEmpty())
    {
        if (result.useProxy && !proxyResource)
            NX_WARNING(NX_SCOPE_TAG, "A Resource is required to proxy %1", result.actionUrl);

        if (result.useDeviceCredentials && !authenticator)
            NX_WARNING(NX_SCOPE_TAG, "Can not authenticate %1", result.actionUrl);

        WebViewDialog::showUrl(
            result.actionUrl,
            /*enableClientApi*/ true,
            context,
            result.useProxy ? proxyResource : QnResourcePtr{},
            result.useDeviceCredentials ? authenticator : nullptr,
            /*checkCertificate*/ !result.useDeviceCredentials,
            parent);
    }
}

std::optional<AnalyticsActionsHelper::SettingsValuesMap>
    AnalyticsActionsHelper::requestSettingsMap(
        const QJsonObject& settingsModel,
        QWidget* parent)
{
    const auto values = requestSettingsJson(settingsModel, parent);
    if (!values)
        return std::nullopt;

    SettingsValuesMap result;
    for (const auto& key: values->keys())
        result[key] = nx::toString(values->value(key));

    return result;
}

std::optional<QJsonObject> AnalyticsActionsHelper::requestSettingsJson(
    const QJsonObject& settingsModel,
    QWidget* parent)
{
    return settingsModel.isEmpty()
        ? QJsonObject{}
        : AnalyticsActionSettingsDialog::request(settingsModel, parent);
}

} // namespace nx::vms::client::desktop
