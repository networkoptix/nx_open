// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_action_settings_dialog.h"

#include <nx/vms/client/core/utils/qml_helpers.h>

namespace nx::vms::client::desktop {

AnalyticsActionSettingsDialog::AnalyticsActionSettingsDialog(
    const QJsonObject& settingsModel,
    const QJsonObject& initialValues,
    QWidget* parent)
    :
    QmlDialogWrapper(
        QUrl("Nx/InteractiveSettings/SettingsDialog.qml"),
        QVariantMap{{"settingsModel", settingsModel}, {"initialValues", initialValues}},
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

void AnalyticsActionSettingsDialog::request(
    const QJsonObject& settingsModel,
    std::function<void(std::optional<QJsonObject>)> requestHandler,
    const QJsonObject& initialValues,
    QWidget* parent)
{
    if(!NX_ASSERT(requestHandler))
        return;

    auto dialog = new AnalyticsActionSettingsDialog(settingsModel, initialValues, parent);
    connect(
        dialog,
        &QmlDialogWrapper::done,
        dialog,
        [dialog, handler = std::move(requestHandler)](bool accepted)
        {
            if (accepted)
                handler(dialog->getValues());
            else
                handler({});

            dialog->deleteLater();
        });

    dialog->open();
}

} // namespace nx::vms::client::desktop
