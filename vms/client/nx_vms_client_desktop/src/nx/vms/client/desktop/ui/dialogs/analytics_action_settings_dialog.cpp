// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_action_settings_dialog.h"

#include <nx/vms/client/core/utils/qml_helpers.h>

namespace nx::vms::client::desktop {

AnalyticsActionSettingsDialog::AnalyticsActionSettingsDialog(
    const QJsonObject& settingsModel,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/InteractiveSettings/SettingsDialog.qml"),
        QVariantMap{{"settingsModel", settingsModel}},
        parent),
    QnSessionAwareDelegate(parent)
{
}

QJsonObject AnalyticsActionSettingsDialog::getValues() const
{
    return QJsonObject::fromVariantMap(
        core::invokeQmlMethod<QVariantMap>(rootObjectHolder()->object(), "getValues"));
}

bool AnalyticsActionSettingsDialog::tryClose(bool /*force*/)
{
    reject();
    return true;
}

void AnalyticsActionSettingsDialog::forcedUpdate()
{
}

std::optional<QJsonObject> AnalyticsActionSettingsDialog::request(
    const QJsonObject& settingsModel,
    QWidget* parent)
{
    AnalyticsActionSettingsDialog dialog(settingsModel, parent);
    if (!dialog.exec())
        return {};

    return dialog.getValues();
}

} // nx::vms::client::desktop
