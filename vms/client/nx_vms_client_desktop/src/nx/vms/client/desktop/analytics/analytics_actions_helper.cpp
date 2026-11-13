// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_actions_helper.h"

#include <QtGui/QAction>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/analytics/action_type_descriptor_manager.h>
#include <nx/analytics/taxonomy/abstract_state.h>
#include <nx/analytics/taxonomy/object_type.h>
#include <nx/network/http/http_types.h>
#include <nx/utils/log/log_main.h>
#include <nx/vms/api/analytics/descriptors.h>
#include <nx/vms/client/desktop/common/dialogs/web_view_dialog.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/ui/dialogs/analytics_action_settings_dialog.h>
#include <nx/vms/client/desktop/window_context.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

namespace {

void sendObjectAction(WindowContext* context,
    const QnVirtualCameraResourcePtr& camera,
    const api::AnalyticsAction& actionData)
{
    const auto systemContext = SystemContext::fromResource(camera);
    const auto api = systemContext ? systemContext->connectedServerApi() : nullptr;
    if (!api)
        return;

    api->executeAnalyticsAction(
        actionData,
        [context, camera, engineId = actionData.engineId](
            bool success, rest::Handle /*requestId*/, nx::network::rest::JsonResult result)
        {
            if (!success || result.errorId != nx::network::rest::ErrorId::ok)
            {
                QnSessionAwareMessageBox::warning(context->mainWindowWidget(),
                    AnalyticsActionsHelper::tr("Failed to execute plugin action"),
                    result.errorString);
                return;
            }

            const auto systemContext = SystemContext::fromResource(camera);
            if (!systemContext)
                return;

            AnalyticsActionsHelper::processResult(
                result.deserialized<api::AnalyticsActionResult>(),
                context,
                systemContext->resourcePool()->getResourceById(engineId),
                /*authenticator*/ {},
                context->mainWindowWidget());
        },
        context);
}

} // namespace

QList<QAction*> AnalyticsActionsHelper::createObjectActions(
    const QnVirtualCameraResourcePtr& camera,
    const std::string& objectTypeId,
    ObjectActionHandler onTriggered,
    QObject* parent)
{
    const auto systemContext = camera->systemContext();
    if (!NX_ASSERT(systemContext))
        return {};

    QList<QAction*> result;

    const nx::analytics::ActionTypeDescriptorManager descriptorManager(
        systemContext->resourcePool(), systemContext->runtimeInfoManager());
    const auto actionsByEngine =
        descriptorManager.availableObjectActionTypeDescriptors(objectTypeId, camera);

    // The server resolves the track through the analytics DB before anything else, and objects of
    // these types are never indexed there, so every action with requirements would fail.
    const auto taxonomyState = systemContext->analyticsTaxonomyState();
    const auto objectType = taxonomyState ? taxonomyState->objectTypeById(objectTypeId) : nullptr;
    const bool skipArchiveDependentActions =
        objectType && (objectType->isLiveOnly() || objectType->isNonIndexable());

    for (const auto& [engineId, actionById]: actionsByEngine)
    {
        QList<QAction*> engineActions;
        for (const auto& [actionId, actionType]: actionById)
        {
            if (skipArchiveDependentActions && actionType.requirements.capabilities)
                continue;

            const auto action = new QAction(QString::fromStdString(actionType.name), parent);
            QObject::connect(action,
                &QAction::triggered,
                action,
                [onTriggered, engineId = engineId, actionType = actionType]
                { onTriggered(engineId, actionType); });

            engineActions.push_back(action);
        }

        if (engineActions.isEmpty())
            continue;

        if (!result.isEmpty())
        {
            auto separator = new QAction(parent);
            separator->setSeparator(true);
            result.push_back(separator);
        }

        result.append(engineActions);
    }

    return result;
}

void AnalyticsActionsHelper::executeObjectAction(WindowContext* context,
    nx::Uuid engineId,
    const api::analytics::ActionTypeDescriptor& actionType,
    nx::Uuid trackId,
    const QnVirtualCameraResourcePtr& camera,
    std::chrono::microseconds timestamp)
{
    const auto systemContext = SystemContext::fromResource(camera);
    if (!systemContext || !systemContext->connectedServerApi())
        return;

    api::AnalyticsAction actionData;
    actionData.engineId = engineId;
    actionData.actionId = QString::fromStdString(actionType.id);
    actionData.objectTrackId = trackId;
    actionData.deviceId = toString(camera->getId());
    actionData.timestampUs = timestamp;

    if (actionType.parametersModel.isEmpty())
    {
        sendObjectAction(context, camera, actionData);
        return;
    }

    requestSettingsMap(
        actionType.parametersModel,
        [context, camera, actionData = std::move(actionData)](
            std::optional<SettingsValuesMap> params) mutable
        {
            if (!params)
                return;

            for (auto&& [k, v]: params->asKeyValueRange())
                actionData.parameters.emplace(k, std::move(v));

            sendObjectAction(context, camera, actionData);
        },
        context->mainWindowWidget());
}

void AnalyticsActionsHelper::processResult(
    const api::AnalyticsActionResult& result,
    WindowContext* context,
    const QnResourcePtr& proxyResource,
    std::shared_ptr<AbstractWebAuthenticator> authenticator,
    QWidget* parent)
{
    if (!result.messageToUser.isEmpty())
    {
        QnSessionAwareMessageBox message(parent);
        message.setIcon(QnMessageBox::Icon::Success);
        message.setText(result.messageToUser);
        message.setStandardButtons(QDialogButtonBox::Ok);
        message.setDefaultButton(QDialogButtonBox::NoButton);
        setHelpTopic(&message, HelpTopic::Id::Forced_Empty);
        message.exec();
    }

    if (!result.actionUrl.isEmpty())
    {
        if (result.useProxy && !proxyResource)
            NX_WARNING(NX_SCOPE_TAG, "A Resource is required to proxy %1", result.actionUrl);

        if (result.useDeviceCredentials && !authenticator)
            NX_WARNING(NX_SCOPE_TAG, "Can not authenticate %1", result.actionUrl);

        auto webDialog = createSelfDestructingDialog<QnSessionAware<WebViewDialog>>(parent);
        webDialog->init(
            result.actionUrl,
            /*enableClientApi*/ true,
            context,
            result.useProxy ? proxyResource : QnResourcePtr{},
            result.useDeviceCredentials ? authenticator : nullptr,
            /*checkCertificate*/ !result.useDeviceCredentials);

        webDialog->show();
    }
}

void AnalyticsActionsHelper::requestSettingsMap(
        const QJsonObject& settingsModel,
        std::function<void(std::optional<SettingsValuesMap>)> callback,
        QWidget* parent)
{
    if(!NX_ASSERT(callback))
        return;

    requestSettingsJson(
        settingsModel,
        [callback = std::move(callback)](std::optional<QJsonObject> setingsJson)
        {
            if (!setingsJson)
            {
                callback({});
                return;
            }

            SettingsValuesMap result;
            for (const auto& key: setingsJson->keys())
                result[key] = nx::toString(setingsJson->value(key));

            callback(result);
        },
        parent);
}

void AnalyticsActionsHelper::requestSettingsJson(
    const QJsonObject& settingsModel,
    std::function<void(std::optional<QJsonObject>)> callback,
    QWidget* parent)
{
    if(!NX_ASSERT(callback))
        return;

    if (settingsModel.isEmpty())
    {
        callback(QJsonObject{});
        return;
    }

    AnalyticsActionSettingsDialog::request(settingsModel, std::move(callback), {}, parent);
}

} // namespace nx::vms::client::desktop
