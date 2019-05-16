#include "analytics_settings_widget.h"

#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickItem>
#include <QtQml/QQmlProperty>
#include <QtCore/QUrlQuery>
#include <QtCore/QMetaObject>

#include <nx/utils/log/assert.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/client/desktop/analytics/analytics_engines_watcher.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <common/common_module.h>
#include <client_core/connection_context_aware.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <api/server_rest_connection.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

namespace {

const char* kCurrentEngineIdPropertyName = "currentEngineId";

QVariantMap engineInfoToVariantMap(const AnalyticsEnginesWatcher::AnalyticsEngineInfo& info)
{
    return QVariantMap{
        {"id", QVariant::fromValue(info.id)},
        {"name", info.name}
    };
}

bool isEngineVisible(const AnalyticsEnginesWatcher::AnalyticsEngineInfo& info)
{
    const auto settings = info.settingsModel.value("items").toArray();
    return !settings.isEmpty();
}

} // namespace

class AnalyticsSettingsWidget::Private: public QObject, public QnConnectionContextAware
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

    Q_INVOKABLE void setSettingsValues(const QnUuid& engineId, const QJsonObject& values)
    {
        settingsValuesByEngineId.insert(engineId, SettingsValues{values, true});
        hasChanges = true;
        emit q->hasChangesChanged();
    }

    Q_INVOKABLE QJsonObject settingsModel(const QnUuid& engineId)
    {
        return enginesWatcher->engineInfo(engineId).settingsModel;
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
    void applySettingsValues();

signals:
    void analyticsEnginesChanged();
    void settingsValuesChanged(const QnUuid& engineId);
    void loadingChanged();

public slots:
    void onCurrentEngineIdChanged();

private:
    void addEngine(
        const QnUuid& engineId, const AnalyticsEnginesWatcher::AnalyticsEngineInfo& engineInfo);
    void removeEngine(const QnUuid& engineId);
    void updateEngine(const QnUuid& engineId);

public:
    QQuickWidget* view = nullptr;
    AnalyticsEnginesWatcher* enginesWatcher = nullptr;
    QVariantList engines;
    bool hasChanges = false;
    bool settingsLoading = false;
    QList<rest::Handle> pendingRefreshRequests;
    QList<rest::Handle> pendingApplyRequests;

    struct SettingsValues
    {
        QJsonObject values;
        bool changed = false;
    };
    QHash<QnUuid, SettingsValues> settingsValuesByEngineId;
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

    QQmlProperty property(view->rootObject(), kCurrentEngineIdPropertyName);
    property.connectNotifySignal(this, SLOT(onCurrentEngineIdChanged()));

    connect(enginesWatcher, &AnalyticsEnginesWatcher::engineAdded, this, &Private::addEngine);
    connect(enginesWatcher, &AnalyticsEnginesWatcher::engineRemoved, this, &Private::removeEngine);
    connect(enginesWatcher, &AnalyticsEnginesWatcher::engineNameChanged,
        this, &Private::updateEngine);
}

void AnalyticsSettingsWidget::Private::updateEngines()
{
    if (!view->rootObject())
        return;

    engines.clear();

    for (const auto& info: enginesWatcher->engineInfos())
        addEngine(info.id, info);
}

void AnalyticsSettingsWidget::Private::addEngine(
    const QnUuid& /*engineId*/,
    const AnalyticsEnginesWatcher::AnalyticsEngineInfo& engineInfo)
{
    // Hide engines without settings on the model level.
    if (!isEngineVisible(engineInfo))
        return;

    auto it = std::lower_bound(engines.begin(), engines.end(), engineInfo,
        [](const auto& item, const auto& engineInfo)
        {
            return item.toMap().value("name").toString() < engineInfo.name;
        });

    engines.insert(it, engineInfoToVariantMap(engineInfo));

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

    *it = engineInfoToVariantMap(enginesWatcher->engineInfo(engineId));

    emit analyticsEnginesChanged();
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

    const auto server = commonModule()->currentServer();
    if (!NX_ASSERT(server))
        return;

    const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
    if (!NX_ASSERT(engine))
        return;

    const auto handle = server->restConnection()->getEngineAnalyticsSettings(
        engine,
        nx::utils::guarded(this,
            [this, engineId](
                bool success,
                rest::Handle requestId,
                const nx::vms::api::analytics::SettingsResponse& result)
            {
                if (!pendingRefreshRequests.removeOne(requestId))
                    return;

                setLoading(!pendingRefreshRequests.isEmpty());

                if (!success)
                    return;

                settingsValuesByEngineId[engineId] = SettingsValues{result.values, false};
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

    const auto server = commonModule()->currentServer();
    if (!NX_ASSERT(server))
        return;

    for (auto it = settingsValuesByEngineId.begin(); it != settingsValuesByEngineId.end(); ++it)
    {
        if (!it->changed)
            continue;

        const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(it.key());
        if (!NX_ASSERT(engine))
            continue;

        const auto handle = server->restConnection()->setEngineAnalyticsSettings(
            engine,
            it->values,
            nx::utils::guarded(this,
                [this, engineId = it.key()](
                    bool success,
                    rest::Handle requestId,
                    const nx::vms::api::analytics::SettingsResponse& result)
                {
                    if (!pendingApplyRequests.removeOne(requestId))
                        return;

                    setLoading(!pendingApplyRequests.isEmpty());

                    if (!success)
                        return;

                    settingsValuesByEngineId[engineId] = SettingsValues{result.values, false};
                    emit settingsValuesChanged(engineId);
                }),
            thread());

        if (handle <= 0)
            continue;

        pendingApplyRequests.append(handle);
    }

    setLoading(!pendingApplyRequests.isEmpty());
}

void AnalyticsSettingsWidget::Private::onCurrentEngineIdChanged()
{
    refreshSettingsValues(
        view->rootObject()->property(kCurrentEngineIdPropertyName).value<QnUuid>());
}

AnalyticsSettingsWidget::AnalyticsSettingsWidget(QWidget* parent):
    base_type(parent),
    d(new Private(this))
{
    anchorWidgetToParent(d->view);
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

} // namespace nx::vms::client::desktop

#include "analytics_settings_widget.moc"
