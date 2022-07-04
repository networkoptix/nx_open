// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_actions_helper.h"

#include <QtCore/QJsonObject>
#include <QtCore/QJsonDocument>
#include <QtCore/QMetaObject>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QScrollArea>

#include <client_core/client_core_module.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

void AnalyticsSettingsActionsHelper::processResult(
    const AnalyticsActionResult& result,
    QnWorkbenchContext* context,
    QWidget* parent)
{
    if (!result.messageToUser.isEmpty())
        QnMessageBox::success(parent, result.messageToUser);

    if (!result.actionUrl.isEmpty())
        WebViewDialog::showUrl(QUrl(result.actionUrl), /*enableClientApi*/ true, context, parent);
}

std::optional<AnalyticsSettingsActionsHelper::SettingsValues>
    AnalyticsSettingsActionsHelper::requestSettings(
        const QJsonObject& settingsModel, QWidget* parent)
{
    if (settingsModel.isEmpty())
        return SettingsValues{};

    QnMessageBox parametersDialog(parent);
    parametersDialog.addButton(QDialogButtonBox::Ok);
    parametersDialog.addButton(QDialogButtonBox::Cancel);
    parametersDialog.setText(tr("Enter parameters"));
    parametersDialog.setInformativeText(tr("Action requires some parameters to be filled."));
    parametersDialog.setIcon(QnMessageBoxIcon::Information);

    const auto view = new QQuickWidget(qnClientCoreModule->mainQmlEngine(), &parametersDialog);
    view->setClearColor(parametersDialog.palette().window().color());
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl("Nx/InteractiveSettings/SettingsView.qml"));

    const auto root = view->rootObject();
    if (!NX_ASSERT(root))
        return {};

    QMetaObject::invokeMethod(
        root,
        "loadModel",
        Qt::DirectConnection,
        Q_ARG(QVariant, settingsModel.toVariantMap()),
        /*initialValues*/ Q_ARG(QVariant, {}),
        /*restoreScrollPosition*/ Q_ARG(QVariant, false));

    const auto panel = new QScrollArea(&parametersDialog);
    panel->setFixedHeight(400);
    const auto layout = new QHBoxLayout(panel);
    layout->addWidget(view);

    parametersDialog.addCustomWidget(panel, QnMessageBox::Layout::Main);
    if (parametersDialog.exec() != QDialogButtonBox::Ok)
        return {};

    QVariant result;
    QMetaObject::invokeMethod(
        root,
        "getValues",
        Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, result));

    SettingsValues settingsValues;
    const auto resultMap = result.value<QVariantMap>();
    for (auto iter = resultMap.cbegin(); iter != resultMap.cend(); ++iter)
        settingsValues.insert(iter.key(), iter.value().toString());

    return settingsValues;
}

std::optional<QJsonObject> AnalyticsSettingsActionsHelper::requestSettingsJson(
    const QJsonObject& settingsModel,
    QWidget* parent)
{
    const auto settings = requestSettings(settingsModel, parent);
    if (!settings)
        return std::nullopt;

    QJsonObject result;
    for (const auto& key: settings->keys())
        result[key] = settings->value(key);

    return result;
}

} // namespace nx::vms::client::desktop
