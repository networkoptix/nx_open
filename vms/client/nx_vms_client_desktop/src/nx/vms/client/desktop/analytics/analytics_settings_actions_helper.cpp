// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_actions_helper.h"

#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/ui/dialogs/analytics_action_settings_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

void AnalyticsSettingsActionsHelper::processResult(
    const AnalyticsActionResult& result,
    QnWorkbenchContext* context,
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

        setHelpTopic(&message, Qn::Forced_Empty_Help);
        message.exec();
    }

    if (!result.actionUrl.isEmpty())
        WebViewDialog::showUrl(QUrl(result.actionUrl), /*enableClientApi*/ true, context, parent);
}

std::optional<AnalyticsSettingsActionsHelper::SettingsValuesMap>
    AnalyticsSettingsActionsHelper::requestSettingsMap(
        const QJsonObject& settingsModel,
        QWidget* parent)
{
    const auto values = requestSettingsJson(settingsModel, parent);
    if (!values)
        return std::nullopt;

    SettingsValuesMap result;
    for (const auto& key: values->keys())
        result[key] = values->value(key).toString();

    return result;
}

std::optional<QJsonObject> AnalyticsSettingsActionsHelper::requestSettingsJson(
    const QJsonObject& settingsModel,
    QWidget* parent)
{
    return settingsModel.isEmpty()
        ? QJsonObject{}
        : AnalyticsActionSettingsDialog::request(settingsModel, parent);
}

} // namespace nx::vms::client::desktop
