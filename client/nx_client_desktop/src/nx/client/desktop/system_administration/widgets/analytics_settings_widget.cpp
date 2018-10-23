#include "analytics_settings_widget.h"

#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickItem>
#include <QtCore/QUrlQuery>
#include <QtCore/QMetaObject>

#include <nx/utils/log/assert.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/client/desktop/analytics/analytics_engines_watcher.h>
#include <nx/client/desktop/common/utils/widget_anchor.h>
#include <client_core/connection_context_aware.h>
#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>

using namespace nx::vms::common;

namespace nx::client::desktop {

namespace {

QVariantMap engineInfoToVariantMap(const AnalyticsEnginesWatcher::AnalyticsEngineInfo& info)
{
    return QVariantMap{
        {"id", QVariant::fromValue(info.id)},
        {"name", info.name}
    };
}

} // namespace

class AnalyticsSettingsWidget::Private: public QObject, public QnConnectionContextAware
{
    Q_OBJECT
    Q_PROPERTY(QVariant analyticsEngines READ analyticsEngines NOTIFY analyticsEnginesChanged)

    AnalyticsSettingsWidget* const q = nullptr;

public:
    Private(AnalyticsSettingsWidget* q);
    void updateEngines();

    Q_INVOKABLE QVariant settingsValues(const QnUuid& engineId)
    {
        QVariantMap values = settingsValuesByEngineId.value(engineId);
        if (!values.isEmpty())
            return values;

        const auto engine = resourcePool()->getResourceById<AnalyticsEngineResource>(engineId);
        if (!NX_ASSERT(engine))
            return {};

        values = engine->settingsValues();
        settingsValuesByEngineId.insert(engineId, values);
        return values;
    }

    Q_INVOKABLE void setSettingsValues(const QnUuid& engineId, const QVariantMap& values)
    {
        settingsValuesByEngineId.insert(engineId, values);
        hasChanges = true;
        emit q->hasChangesChanged();
    }

    Q_INVOKABLE QJsonObject settingsModel(const QnUuid& engineId)
    {
        return enginesWatcher->engineInfo(engineId).settingsModel;
    }

    QVariant analyticsEngines() const { return engines; }

    void activateEngine(const QnUuid& engineId);

signals:
    void analyticsEnginesChanged();

private:
    void addEngine(
        const QnUuid& engineId, const AnalyticsEnginesWatcher::AnalyticsEngineInfo& engineInfo);
    void removeEngine(const QnUuid& engineId);
    void updateEngine(const QnUuid& engineId);

public:
    QQuickWidget* view = nullptr;
    AnalyticsEnginesWatcher* enginesWatcher = nullptr;
    QVariantList engines;
    QHash<QnUuid, QVariantMap> settingsValuesByEngineId;
    bool hasChanges = false;
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
    const auto resourcePool = d->resourcePool();

    for (auto it = d->settingsValuesByEngineId.begin();
        it != d->settingsValuesByEngineId.end();
        ++it)
    {
        if (const auto engine = resourcePool->getResourceById<AnalyticsEngineResource>(it.key()))
            engine->setSettingsValues(it.value());
    }

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

} // namespace nx::client::desktop

#include "analytics_settings_widget.moc"
