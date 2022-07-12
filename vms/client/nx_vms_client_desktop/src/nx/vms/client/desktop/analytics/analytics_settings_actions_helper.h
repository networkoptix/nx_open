// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include <QtCore/QCoreApplication>

#include <api/model/analytics_actions.h>

class QWidget;
class QnWorkbenchContext;

namespace nx::vms::client::desktop {

class AnalyticsSettingsActionsHelper
{
    Q_DECLARE_TR_FUNCTIONS(AnalyticsSettingsActionsHelper)
public:
    static void processResult(
        const AnalyticsActionResult& result,
        QnWorkbenchContext* context = nullptr,
        QWidget* parent = nullptr);

    using SettingsValues = QMap<QString, QString>;
    static std::optional<SettingsValues> requestSettings(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);

    static std::optional<QJsonObject> requestSettingsJson(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
