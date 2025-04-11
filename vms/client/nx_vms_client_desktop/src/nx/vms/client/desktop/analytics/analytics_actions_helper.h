// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QJsonObject>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/analytics_actions.h>
#include <nx/vms/client/desktop/common/utils/abstract_web_authenticator.h>

class QWidget;

namespace nx::vms::client::desktop {

class AbstractWebAuthenticator;
class WindowContext;

class AnalyticsActionsHelper
{
public:
    /**
     * Processes an Action result.
     * @param result Action result to process.
     * @param context Workbench context.
     * @param proxyResource Resource, which should be used while proxying.
     * @param authenticator Authenticator, which should be used during the authentication.
     * @param parent Dialog's parent.
     */
    static void processResult(
        const api::AnalyticsActionResult& result,
        WindowContext* context = nullptr,
        const QnResourcePtr& proxyResource = {},
        std::shared_ptr<AbstractWebAuthenticator> authenticator = nullptr,
        QWidget* parent = nullptr);

    using SettingsValuesMap = QMap<QString, QString>;
    static void requestSettingsMap(
        const QJsonObject& settingsModel,
        std::function<void(std::optional<SettingsValuesMap>)> callback,
        QWidget* parent = nullptr);

    static void requestSettingsJson(
        const QJsonObject& settingsModel,
        std::function<void(std::optional<QJsonObject>)> callback,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
