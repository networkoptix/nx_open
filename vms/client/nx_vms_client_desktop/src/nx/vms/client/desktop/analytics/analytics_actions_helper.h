// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>
#include <optional>

#include <QtCore/QJsonObject>

#include <api/model/analytics_actions.h>
#include <core/resource/resource_fwd.h>
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
        const AnalyticsActionResult& result,
        WindowContext* context = nullptr,
        const QnResourcePtr& proxyResource = {},
        std::shared_ptr<AbstractWebAuthenticator> authenticator = nullptr,
        QWidget* parent = nullptr);

    using SettingsValuesMap = QMap<QString, QString>;
    static std::optional<SettingsValuesMap> requestSettingsMap(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);

    static std::optional<QJsonObject> requestSettingsJson(
        const QJsonObject& settingsModel,
        QWidget* parent = nullptr);
};

} // namespace nx::vms::client::desktop
