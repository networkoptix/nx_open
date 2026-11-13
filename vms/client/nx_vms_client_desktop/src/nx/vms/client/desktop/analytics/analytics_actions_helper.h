// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <QtCore/QCoreApplication>
#include <QtCore/QJsonObject>
#include <QtCore/QList>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/analytics/analytics_actions.h>
#include <nx/vms/client/desktop/common/utils/abstract_web_authenticator.h>

class QAction;
class QWidget;

namespace nx::vms::api::analytics { struct ActionTypeDescriptor; }

namespace nx::vms::client::desktop {

class AbstractWebAuthenticator;
class WindowContext;

class AnalyticsActionsHelper
{
    Q_DECLARE_TR_FUNCTIONS(AnalyticsActionsHelper)

public:
    using ObjectActionHandler = std::function<void(
        nx::Uuid engineId, const api::analytics::ActionTypeDescriptor& actionType)>;

    /**
     * Creates the Object Actions available for the object type on the camera, grouped by engine
     * with separators in between.
     * @param parent Parent of the created actions.
     */
    static QList<QAction*> createObjectActions(const QnVirtualCameraResourcePtr& camera,
        const std::string& objectTypeId,
        ObjectActionHandler onTriggered,
        QObject* parent);

    /**
     * Executes an Object Action on the given track: asks for the action parameters if the
     * action has any, sends the request and shows the result.
     * @param timestamp Video frame from which the action is triggered.
     */
    static void executeObjectAction(WindowContext* context,
        nx::Uuid engineId,
        const api::analytics::ActionTypeDescriptor& actionType,
        nx::Uuid trackId,
        const QnVirtualCameraResourcePtr& camera,
        std::chrono::microseconds timestamp);

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
