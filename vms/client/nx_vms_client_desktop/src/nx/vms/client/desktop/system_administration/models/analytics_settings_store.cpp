// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_store.h"

#include <QtQml/QQmlEngine>
#include <QtWidgets/QWidget>

#include <api/server_rest_connection.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/resource/analytics_plugin_resource.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

bool isEngineVisible(const AnalyticsEngineInfo& info)
{
    // Device-dependent plugins without settings must be hidden.
    if (!info.isDeviceDependent)
        return true;

    const auto settings = info.settingsModel.value("items").toArray();
    return !settings.isEmpty();
}

} // namespace

AnalyticsSettingsStore::AnalyticsSettingsStore(QWidget* parent):
    QObject(parent),
    CurrentSystemContextAware(parent),
    m_parent(parent),
    m_enginesWatcher(new AnalyticsEnginesWatcher(this))
{
    connect(m_enginesWatcher.get(), &AnalyticsEnginesWatcher::engineAdded,
        this, &AnalyticsSettingsStore::addEngine);

    connect(m_enginesWatcher.get(), &AnalyticsEnginesWatcher::engineRemoved,
        this, &AnalyticsSettingsStore::removeEngine);

    connect(m_enginesWatcher.get(), &AnalyticsEnginesWatcher::engineUpdated,
        this, &AnalyticsSettingsStore::updateEngine);

    connect(m_enginesWatcher.get(), &AnalyticsEnginesWatcher::engineSettingsModelChanged, this,
        [this](const QnUuid& engineId)
        {
            if (engineId == m_currentEngineId)
                emit currentSettingsStateChanged();
        });

    const auto serviceManager = systemContext()->saasServiceManager();
    connect(serviceManager, &common::saas::ServiceManager::dataChanged,
        this, &AnalyticsSettingsStore::licenseSummariesChanged);
}

void AnalyticsSettingsStore::updateEngines()
{
    if (!m_engines.empty())
        return;

    for (const auto& info: m_enginesWatcher->engineInfos())
    {
        // TODO: VMS-26953: Rewrite this function. Engines should be updated in one change, not
        // one-by-one.
        addEngine(info.id, info);
    }
}

void AnalyticsSettingsStore::setCurrentEngineId(const QnUuid& engineId)
{
    m_currentEngineId = engineId;
    emit currentSettingsStateChanged();
    refreshSettings(m_currentEngineId);
}

QJsonObject AnalyticsSettingsStore::settingsValues(const QnUuid& engineId)
{
    return m_settingsValuesByEngineId.value(engineId).values;
}

QVariant AnalyticsSettingsStore::requestParameters(const QJsonObject& model)
{
    const auto values = AnalyticsActionsHelper::requestSettingsJson(model, m_parent);
    return values ? *values : QVariant{};
}

void AnalyticsSettingsStore::setSettingsValues(
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& values,
    const QJsonObject& parameters)
{
    m_settingsValuesByEngineId.insert(engineId, SettingsValues{values, true});

    if (!activeElement.isEmpty())
        activeElementChanged(m_currentEngineId, activeElement, parameters);

    updateHasChanges();
}

QJsonObject AnalyticsSettingsStore::settingsModel(const QnUuid& engineId)
{
    return m_settingsModelByEngineId.value(engineId);
}

void AnalyticsSettingsStore::addEngine(
    const QnUuid& /*engineId*/, const AnalyticsEngineInfo& engineInfo)
{
    // Hide device-dependent engines without settings on the model level.
    if (!isEngineVisible(engineInfo))
        return;

    auto it = std::lower_bound(m_engines.begin(), m_engines.end(), engineInfo,
        [](const auto& item, const auto& engineInfo)
        {
            return item.toMap().value("name").toString() < engineInfo.name;
        });

    m_engines.insert(it, engineInfo.toVariantMap());

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsStore::removeEngine(const QnUuid& engineId)
{
    auto it = std::find_if(m_engines.begin(), m_engines.end(),
        [&engineId](const auto& item)
        {
            return item.toMap().value("id").template value<QnUuid>() == engineId;
        });

    if (it == m_engines.end())
        return;

    m_engines.erase(it);

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsStore::setErrors(const QnUuid& engineId, const QJsonObject& errors)
{
    if (m_errors.value(engineId) == errors)
        return;

    m_errors[engineId] = errors;

    if (engineId == m_currentEngineId)
        emit currentErrorsChanged();
}

void AnalyticsSettingsStore::resetSettings(
    const QnUuid& engineId,
    const QJsonObject& model,
    const QJsonObject& values,
    const QJsonObject& errors,
    bool changed)
{
    m_settingsValuesByEngineId[engineId] = SettingsValues{values, changed};
    m_settingsModelByEngineId[engineId] = model;

    if (engineId == m_currentEngineId)
        emit currentSettingsStateChanged();

    setErrors(engineId, errors);
    updateHasChanges();
}

void AnalyticsSettingsStore::updateEngine(const QnUuid& engineId)
{
    auto it = std::find_if(m_engines.begin(), m_engines.end(),
        [&engineId](const auto& item)
        {
            return item.toMap().value("id").template value<QnUuid>() == engineId;
        });

    if (it == m_engines.end())
        return;

    const auto engineInfo = m_enginesWatcher->engineInfo(engineId);
    // Hide device-dependent engines without settings on the model level.
    if (!isEngineVisible(engineInfo))
        m_engines.erase(it);
    else
        *it = engineInfo.toVariantMap();

    emit analyticsEnginesChanged();
}

void AnalyticsSettingsStore::setLoading(bool loading)
{
    if (m_settingsLoading == loading)
        return;

    m_settingsLoading = loading;
    emit loadingChanged();
}

void AnalyticsSettingsStore::refreshSettings(const QnUuid& engineId)
{
    if (engineId.isNull())
        return;

    if (m_settingsValuesByEngineId.contains(engineId))
        return;

    if (!m_pendingApplyRequests.isEmpty())
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
                if (!m_pendingRefreshRequests.removeOne(requestId))
                    return;

                setLoading(!m_pendingRefreshRequests.isEmpty());

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

    m_pendingRefreshRequests.append(handle);
    setLoading(true);
}

void AnalyticsSettingsStore::applySettingsValues()
{
    if (!m_pendingApplyRequests.isEmpty() || !m_pendingRefreshRequests.isEmpty())
        return;

    if (!NX_ASSERT(connection()))
        return;

    for (auto it = m_settingsValuesByEngineId.begin(); it != m_settingsValuesByEngineId.end(); ++it)
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
                    if (!m_pendingApplyRequests.removeOne(requestId))
                        return;

                    setLoading(!m_pendingApplyRequests.isEmpty());

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

        m_pendingApplyRequests.append(handle);
    }

    setLoading(!m_pendingApplyRequests.isEmpty());
}

void AnalyticsSettingsStore::discardChanges()
{
    m_settingsValuesByEngineId.clear();
    updateHasChanges();
    if (auto api = connectedServerApi())
    {
        for (auto handle: m_pendingApplyRequests)
            api->cancelRequest(handle);
        for (auto handle: m_pendingRefreshRequests)
            api->cancelRequest(handle);
    }
    m_pendingApplyRequests.clear();
    m_pendingRefreshRequests.clear();
}

void AnalyticsSettingsStore::activeElementChanged(
    const QnUuid& engineId,
    const QString& activeElement,
    const QJsonObject& parameters)
{
    if (!m_pendingApplyRequests.isEmpty() || !m_pendingRefreshRequests.isEmpty())
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
        m_settingsValuesByEngineId[engineId].values,
        parameters,
        nx::utils::guarded(this,
            [this, engine, engineId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::EngineActiveSettingChangedResponse& result)
            {
                if (!m_pendingApplyRequests.removeOne(requestId))
                    return;

                setLoading(!m_pendingApplyRequests.isEmpty());

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
                    windowContext(),
                    engine,
                    /*authenticator*/ {},
                    m_parent);
            }),
            thread());

    if (handle <= 0)
        return;

    m_pendingApplyRequests.append(handle);

    setLoading(!m_pendingApplyRequests.isEmpty());
}

void AnalyticsSettingsStore::updateHasChanges()
{
    m_hasChanges =
        std::any_of(m_settingsValuesByEngineId.begin(), m_settingsValuesByEngineId.end(),
            [](const SettingsValues& values) { return values.changed; });

    emit hasChangesChanged();
}

bool AnalyticsSettingsStore::isNetworkRequestRunning() const
{
    return !m_pendingApplyRequests.empty() || !m_pendingRefreshRequests.empty();
}

ApiIntegrationRequestsModel*
    AnalyticsSettingsStore::makeApiIntegrationRequestsModel() const
{
    if (!ini().enableMetadataApi)
        return nullptr;

    auto requestsModel = new ApiIntegrationRequestsModel(systemContext(), m_parent);
    QQmlEngine::setObjectOwnership(requestsModel, QQmlEngine::CppOwnership);
    return requestsModel;
}

EngineLicenseSummaryProvider* AnalyticsSettingsStore::makeEngineLicenseSummaryProvider() const
{
    auto licenseProvider = new EngineLicenseSummaryProvider(systemContext(), m_parent);
    QQmlEngine::setObjectOwnership(licenseProvider, QQmlEngine::CppOwnership);
    return licenseProvider;
}

saas::ServiceManager* AnalyticsSettingsStore::saasServiceManager() const
{
    saas::ServiceManager* saasServiceManager = systemContext()->saasServiceManager();
    QQmlEngine::setObjectOwnership(saasServiceManager, QQmlEngine::CppOwnership);
    return saasServiceManager;
}

} // namespace nx::vms::client::desktop
