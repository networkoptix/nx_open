// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_widget_p.h"

#include <QtCore/QMetaObject>
#include <QtQuick/QQuickItem>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/engine_license_summary_provider.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>
#include <nx/vms/common/saas/saas_service_manager.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

bool isEngineVisible(const core::AnalyticsEngineInfo& info)
{
    // Device-dependent plugins without settings must be hidden.
    if (!info.isDeviceDependent)
        return true;

    const auto settings = info.settingsModel.value("items").toArray();
    return !settings.isEmpty();
}

} // namespace

AnalyticsSettingsWidget::Private::Private(AnalyticsSettingsWidget* q):
    q(q),
    view(new QQuickWidget(appContext()->qmlEngine(), q)),
    enginesWatcher(new core::AnalyticsEnginesWatcher(this))
{
    view->setClearColor(q->palette().window().color());
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl("Nx/Dialogs/SystemSettings/AnalyticsSettings.qml"));

    installEventHandler(q, {QEvent::Show, QEvent::Hide}, this,
        [this, q]() { view->rootObject()->setVisible(q->isVisible()); });

    if (!NX_ASSERT(view->rootObject()))
        return;

    view->rootObject()->setProperty("store", QVariant::fromValue(this));

    const auto engineLicenseSummaryProvider =
        new EngineLicenseSummaryProvider{q->systemContext(), this};
    view->rootObject()->setProperty(
        "engineLicenseSummaryProvider", QVariant::fromValue(engineLicenseSummaryProvider));

    view->rootObject()->setProperty(
        "saasServiceManager", QVariant::fromValue(systemContext()->saasServiceManager()));

    connect(enginesWatcher.get(), &core::AnalyticsEnginesWatcher::engineAdded,
        this, &Private::addEngine);

    connect(enginesWatcher.get(), &core::AnalyticsEnginesWatcher::engineRemoved,
        this, &Private::removeEngine);

    connect(enginesWatcher.get(), &core::AnalyticsEnginesWatcher::engineUpdated,
        this, &Private::updateEngine);

    connect(enginesWatcher.get(), &core::AnalyticsEnginesWatcher::engineSettingsModelChanged, this,
        [this](const QnUuid& engineId)
        {
            if (engineId == currentEngineId)
                emit currentSettingsStateChanged();
        });

    const auto serviceManager = systemContext()->saasServiceManager();
    connect(serviceManager, &common::saas::ServiceManager::dataChanged,
        this, &Private::licenseSummariesChanged);
}

void AnalyticsSettingsWidget::Private::updateEngines()
{
    if (!view->rootObject())
        return;

    if (!engines.empty())
        return;

    for (const auto& info: enginesWatcher->engineInfos())
    {
        // TODO: VMS-26953: Rewrite this function. Engines should be updated in one change, not
        // one-by-one.
        addEngine(info.id, info);
    }
}

void AnalyticsSettingsWidget::Private::setCurrentEngineId(const QnUuid& engineId)
{
    currentEngineId = engineId;
    emit currentSettingsStateChanged();
    refreshSettings(currentEngineId);
}

QJsonObject AnalyticsSettingsWidget::Private::settingsValues(const QnUuid& engineId)
{
    return settingsValuesByEngineId.value(engineId).values;
}

QVariant AnalyticsSettingsWidget::Private::requestParameters(const QJsonObject& model)
{
    const auto values = AnalyticsActionsHelper::requestSettingsJson(model, /*parent*/ q);
    return values ? *values : QVariant{};
}

void AnalyticsSettingsWidget::Private::setSettingsValues(
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& values,
    const QJsonObject& parameters)
{
    settingsValuesByEngineId.insert(engineId, SettingsValues{values, true});

    if (!activeElement.isEmpty())
        activeElementChanged(currentEngineId, activeElement, parameters);

    updateHasChanges();
}

QJsonObject AnalyticsSettingsWidget::Private::settingsModel(const QnUuid& engineId)
{
    return settingsModelByEngineId.value(engineId);
}

void AnalyticsSettingsWidget::Private::addEngine(
    const QnUuid& /*engineId*/, const core::AnalyticsEngineInfo& engineInfo)
{
    // Hide device-dependent engines without settings on the model level.
    if (!isEngineVisible(engineInfo))
        return;

    auto it = std::lower_bound(engines.begin(), engines.end(), engineInfo,
        [](const auto& item, const auto& engineInfo)
        {
            return item.toMap().value("name").toString() < engineInfo.name;
        });

    engines.insert(it, engineInfo.toVariantMap());

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsWidget::Private::removeEngine(const QnUuid& engineId)
{
    auto it = std::find_if(engines.begin(), engines.end(),
        [&engineId](const auto& item)
        {
            return item.toMap().value("id").template value<QnUuid>() == engineId;
        });

    if (it == engines.end())
        return;

    engines.erase(it);

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsWidget::Private::setErrors(const QnUuid& engineId, const QJsonObject& errors)
{
    if (m_errors.value(engineId) == errors)
        return;

    m_errors[engineId] = errors;

    if (engineId == currentEngineId)
        emit currentErrorsChanged();
}

void AnalyticsSettingsWidget::Private::resetSettings(
    const QnUuid& engineId,
    const QJsonObject& model,
    const QJsonObject& values,
    const QJsonObject& errors,
    bool changed)
{
    settingsValuesByEngineId[engineId] = SettingsValues{values, changed};
    settingsModelByEngineId[engineId] = model;

    if (engineId == currentEngineId)
        emit currentSettingsStateChanged();

    setErrors(engineId, errors);
    updateHasChanges();
}

void AnalyticsSettingsWidget::Private::updateEngine(const QnUuid& engineId)
{
    auto it = std::find_if(engines.begin(), engines.end(),
        [&engineId](const auto& item)
        {
            return item.toMap().value("id").template value<QnUuid>() == engineId;
        });

    if (it == engines.end())
        return;

    const auto engineInfo = enginesWatcher->engineInfo(engineId);
    // Hide device-dependent engines without settings on the model level.
    if (!isEngineVisible(engineInfo))
        engines.erase(it);
    else
        *it = engineInfo.toVariantMap();

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsWidget::Private::activateEngine(const QnUuid& engineId)
{
    if (enginesWatcher->engineInfo(engineId).id.isNull())
        return;

    QMetaObject::invokeMethod(view->rootObject(), "activateEngine",
        Q_ARG(QVariant, QVariant::fromValue(engineId)));
}

void AnalyticsSettingsWidget::Private::setLoading(bool loading)
{
    if (settingsLoading == loading)
        return;

    settingsLoading = loading;
    emit loadingChanged();
}

void AnalyticsSettingsWidget::Private::refreshSettings(const QnUuid& engineId)
{
    if (engineId.isNull())
        return;

    if (settingsValuesByEngineId.contains(engineId))
        return;

    if (!pendingApplyRequests.isEmpty())
        return;

    if (!connection()) //< It may be null if the client just disconnected from the server.
        return;

    const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
    if (!NX_ASSERT(engine))
        return;

    const auto handle = connectedServerApi()->getEngineAnalyticsSettings(
        engine,
        nx::utils::guarded(this,
            [this, engineId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::EngineSettingsResponse& result)
            {
                if (!pendingRefreshRequests.removeOne(requestId))
                    return;

                setLoading(!pendingRefreshRequests.isEmpty());

                if (!success)
                    return;

                resetSettings(
                    engineId,
                    result.settingsModel,
                    result.settingsValues,
                    result.settingsErrors,
                    /*changed*/ false);
            }),
        thread());

    if (handle <= 0)
        return;

    pendingRefreshRequests.append(handle);
    setLoading(true);
}

void AnalyticsSettingsWidget::Private::applySettingsValues()
{
    if (!pendingApplyRequests.isEmpty() || !pendingRefreshRequests.isEmpty())
        return;

    if (!NX_ASSERT(connection()))
        return;

    for (auto it = settingsValuesByEngineId.begin(); it != settingsValuesByEngineId.end(); ++it)
    {
        if (!it->changed)
            continue;

        const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(it.key());
        if (!NX_ASSERT(engine))
            continue;

        const auto handle = connectedServerApi()->setEngineAnalyticsSettings(
            engine,
            it->values,
            nx::utils::guarded(this,
                [this, engineId = it.key()](
                    bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::EngineSettingsResponse& result)
                {
                    if (!pendingApplyRequests.removeOne(requestId))
                        return;

                    setLoading(!pendingApplyRequests.isEmpty());

                    if (!success)
                        return;

                    resetSettings(
                        engineId,
                        result.settingsModel,
                        result.settingsValues,
                        result.settingsErrors,
                        /*changed*/ false);
                }),
            thread());

        if (handle <= 0)
            continue;

        pendingApplyRequests.append(handle);
    }

    setLoading(!pendingApplyRequests.isEmpty());
}

void AnalyticsSettingsWidget::Private::activeElementChanged(
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& parameters)
{
    if (!pendingApplyRequests.isEmpty() || !pendingRefreshRequests.isEmpty())
        return;

    if (!NX_ASSERT(connection()))
        return;

    const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
    if (!NX_ASSERT(engine))
        return;

    const auto handle = connectedServerApi()->engineAnalyticsActiveSettingsChanged(
        engine,
        activeElement,
        settingsModel(engineId),
        settingsValuesByEngineId[engineId].values,
        parameters,
        nx::utils::guarded(this,
            [this, engine, engineId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::EngineActiveSettingChangedResponse& result)
            {
                if (!pendingApplyRequests.removeOne(requestId))
                    return;

                setLoading(!pendingApplyRequests.isEmpty());

                if (!success)
                    return;

                resetSettings(
                    engineId,
                    result.settingsModel,
                    result.settingsValues,
                    result.settingsErrors,
                    /*changed*/ true);

                AnalyticsActionsHelper::processResult(
                    AnalyticsActionResult{
                        .actionUrl = result.actionUrl,
                        .messageToUser = result.messageToUser,
                        .useProxy = result.useProxy,
                        .useDeviceCredentials = result.useDeviceCredentials},
                    q->windowContext(),
                    engine,
                    /*authenticator*/ {},
                    /*parent*/ q);
            }),
            thread());

    if (handle <= 0)
        return;

    pendingApplyRequests.append(handle);

    setLoading(!pendingApplyRequests.isEmpty());
}

void AnalyticsSettingsWidget::Private::updateHasChanges()
{
    hasChanges = std::any_of(settingsValuesByEngineId.begin(), settingsValuesByEngineId.end(),
        [](const SettingsValues& values) { return values.changed; });

    emit q->hasChangesChanged();
}

ApiIntegrationRequestsModel*
    AnalyticsSettingsWidget::Private::makeApiIntegrationRequestsModel() const
{
    return ini().enableMetadataApi
        ? new ApiIntegrationRequestsModel(q->systemContext(), /*parent*/ q)
        : nullptr;
}

} // namespace nx::vms::client::desktop
