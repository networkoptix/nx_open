// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_widget.h"

#include <QtCore/QMetaObject>
#include <QtCore/QScopedPointer>
#include <QtCore/QUrlQuery>
#include <QtQuick/QQuickItem>
#include <QtQuickWidgets/QQuickWidget>

#include <api/server_rest_connection.h>
#include <client_core/client_core_module.h>
#include <common/common_module.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/assert.h>
#include <nx/vms/client/core/common/utils/common_module_aware.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <nx/vms/client/desktop/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/desktop/analytics/analytics_settings_actions_helper.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/utils/qml_property.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>

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

class AnalyticsSettingsWidget::Private:
    public QObject,
    public nx::vms::client::core::CommonModuleAware,
    public nx::vms::client::core::RemoteConnectionAware
{
    Q_OBJECT
    Q_PROPERTY(QVariant analyticsEngines READ analyticsEngines NOTIFY analyticsEnginesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)

    AnalyticsSettingsWidget* const q = nullptr;

public:
    Private(AnalyticsSettingsWidget* q);
    void updateEngines();

    Q_INVOKABLE QJsonObject settingsValues(const QnUuid& engineId)
    {
        return settingsValuesByEngineId.value(engineId).values;
    }

    Q_INVOKABLE void setSettingsValues(
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& paramsModel,
        const QJsonObject& values)
    {
        settingsValuesByEngineId.insert(engineId, SettingsValues{values, true});
        hasChanges = true;

        if (!activeElement.isEmpty())
            activeElementChanged(currentEngineId, activeElement, paramsModel);

        emit q->hasChangesChanged();
    }

    Q_INVOKABLE QJsonObject settingsModel(const QnUuid& engineId)
    {
        if (m_previewSettingsModel[engineId])
            return *m_previewSettingsModel[engineId];

        return settingsModelByEngineId.value(engineId);
    }

    Q_INVOKABLE QJsonObject errors(const QnUuid& engineId)
    {
        return m_errors[engineId];
    }

    QVariant analyticsEngines() const { return engines; }

    void activateEngine(const QnUuid& engineId);

    bool loading() const { return settingsLoading; }

    void setLoading(bool loading)
    {
        if (settingsLoading == loading)
            return;

        settingsLoading = loading;
        emit loadingChanged();
    }

    void refreshSettingsValues(const QnUuid& engineId);
    void activeElementChanged(
        const QnUuid& engineId,
        const QString& activeElement,
        const QJsonObject& paramsModel);

    void applySettingsValues();

signals:
    void analyticsEnginesChanged();
    void settingsValuesChanged(const QnUuid& engineId);
    void settingsModelChanged(const QnUuid& engineId);
    void errorsChanged(const QnUuid& engineId);
    void loadingChanged();

private:
    void addEngine(const QnUuid& engineId, const AnalyticsEngineInfo& engineInfo);
    void removeEngine(const QnUuid& engineId);
    void updateEngine(const QnUuid& engineId);
    void setErrors(const QnUuid& engineId, const QJsonObject& errors);

public:
    const QScopedPointer<QQuickWidget> view;
    const QScopedPointer<AnalyticsEnginesWatcher> enginesWatcher;
    QVariantList engines;
    bool hasChanges = false;
    bool settingsLoading = false;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;
    QHash<QnUuid, std::optional<QJsonObject>> m_previewSettingsModel;
    QHash<QnUuid, QJsonObject> m_errors;

    QmlProperty<QnUuid> currentEngineId{view.data(), "currentEngineId"};

    struct SettingsValues
    {
        QJsonObject values;
        bool changed = false;
    };
    QHash<QnUuid, SettingsValues> settingsValuesByEngineId;
    QHash<QnUuid, QJsonObject> settingsModelByEngineId;
};

AnalyticsSettingsWidget::Private::Private(AnalyticsSettingsWidget* q):
    q(q),
    view(new QQuickWidget(qnClientCoreModule->mainQmlEngine(), q)),
    enginesWatcher(new AnalyticsEnginesWatcher(this))
{
    view->setClearColor(q->palette().window().color());
    view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    view->setSource(QUrl("Nx/Dialogs/SystemSettings/AnalyticsSettings.qml"));

    if (!NX_ASSERT(view->rootObject()))
        return;

    view->rootObject()->setProperty("store", QVariant::fromValue(this));

    currentEngineId.connectNotifySignal(this,
        [this]() { refreshSettingsValues(currentEngineId); });

    connect(enginesWatcher.get(), &AnalyticsEnginesWatcher::engineAdded, this, &Private::addEngine);
    connect(enginesWatcher.get(), &AnalyticsEnginesWatcher::engineRemoved, this, &Private::removeEngine);
    connect(enginesWatcher.get(), &AnalyticsEnginesWatcher::engineUpdated,
        this, &Private::updateEngine);

    connect(enginesWatcher.get(), &AnalyticsEnginesWatcher::engineSettingsModelChanged, this,
        [this](const QnUuid& engineId)
        {
            if (engineId == currentEngineId)
                activateEngine(engineId);
        });
}

void AnalyticsSettingsWidget::Private::updateEngines()
{
    if (!view->rootObject())
        return;

    engines.clear();

    for (const auto& info: enginesWatcher->engineInfos())
    {
        // TODO: VMS-26953: Rewrite this function. Engined should be updated in one change, not
        // one-by-one.
        addEngine(info.id, info);
    }
}

void AnalyticsSettingsWidget::Private::addEngine(
    const QnUuid& /*engineId*/, const AnalyticsEngineInfo& engineInfo)
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

void AnalyticsSettingsWidget::Private::setErrors(const QnUuid& engineId, const QJsonObject& errors)
{
    if (m_errors.value(engineId) == errors)
        return;

    m_errors[engineId] = errors;
    emit errorsChanged(engineId);
}

void AnalyticsSettingsWidget::Private::activateEngine(const QnUuid& engineId)
{
    if (enginesWatcher->engineInfo(engineId).id.isNull())
        return;

    QMetaObject::invokeMethod(view->rootObject(), "activateEngine",
        Q_ARG(QVariant, QVariant::fromValue(engineId)));
}

void AnalyticsSettingsWidget::Private::refreshSettingsValues(const QnUuid& engineId)
{
    if (engineId.isNull())
        return;

    if (settingsValuesByEngineId.contains(engineId))
        return;

    if (!pendingApplyRequests.isEmpty())
        return;

    if (!connection()) //< It may be null if the client just disconnected from the server.
        return;

    m_previewSettingsModel[engineId].reset();

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

                settingsValuesByEngineId[engineId] = SettingsValues{result.settingsValues, false};
                settingsModelByEngineId[engineId] = result.settingsModel;
                emit settingsModelChanged(engineId);
                emit settingsValuesChanged(engineId);
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

                    settingsValuesByEngineId[engineId] =
                        SettingsValues{result.settingsValues, false};
                    emit settingsValuesChanged(engineId);
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
    const QJsonObject& paramsModel)
{
    if (!pendingApplyRequests.isEmpty() || !pendingRefreshRequests.isEmpty())
        return;

    if (!NX_ASSERT(connection()))
        return;

    const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
    if (!NX_ASSERT(engine))
        return;

    const auto paramValues =
        AnalyticsSettingsActionsHelper::requestSettingsJson(paramsModel, /*parent*/ q);

    if (!paramValues)
        return;

    const auto handle = connectedServerApi()->engineAnalyticsActiveSettingsChanged(
        engine,
        activeElement,
        settingsModel(engineId),
        settingsValuesByEngineId[engineId].values,
        *paramValues,
        nx::utils::guarded(this,
            [this, engineId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::EngineActiveSettingChangedResponse& result)
            {
                if (!pendingApplyRequests.removeOne(requestId))
                    return;

                setLoading(!pendingApplyRequests.isEmpty());

                if (!success)
                    return;

                settingsValuesByEngineId[engineId] =
                    SettingsValues{ result.settingsValues, true };

                m_previewSettingsModel[engineId] = result.settingsModel;
                emit settingsModelChanged(engineId);
                setErrors(engineId, result.settingsErrors);

                AnalyticsSettingsActionsHelper::processResult(
                    AnalyticsActionResult{
                        .actionUrl = result.actionUrl,
                        .messageToUser = result.messageToUser},
                    q->context(),
                    /*parent*/ q);
            }),
            thread());

    if (handle <= 0)
        return;

    pendingApplyRequests.append(handle);

    setLoading(!pendingApplyRequests.isEmpty());
}

AnalyticsSettingsWidget::AnalyticsSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    anchorWidgetToParent(d->view.get());

    connect(d.get(), &Private::analyticsEnginesChanged, this,
        &AnalyticsSettingsWidget::visibilityUpdateRequested);
}

AnalyticsSettingsWidget::~AnalyticsSettingsWidget()
{
}

void AnalyticsSettingsWidget::loadDataToUi()
{
    d->activateEngine(QnUuid());
    d->updateEngines();
}

void AnalyticsSettingsWidget::applyChanges()
{
    d->applySettingsValues();
    discardChanges();
}

void AnalyticsSettingsWidget::discardChanges()
{
    d->settingsValuesByEngineId.clear();
    d->hasChanges = false;
}

bool AnalyticsSettingsWidget::hasChanges() const
{
    return d->hasChanges;
}

bool AnalyticsSettingsWidget::activate(const QUrl& url)
{
    const QUrlQuery urlQuery(url);

    const auto& engineId = QnUuid::fromStringSafe(urlQuery.queryItemValue("engineId"));
    if (!engineId.isNull())
        d->activateEngine(engineId);

    return true;
}

bool AnalyticsSettingsWidget::shouldBeVisible() const
{
    return !d->engines.empty();
}

} // namespace nx::vms::client::desktop

// Force Q_OBJECT macro work in the cpp file. Implemented the same way as it was in QMake.
#include "analytics_settings_widget.moc"
