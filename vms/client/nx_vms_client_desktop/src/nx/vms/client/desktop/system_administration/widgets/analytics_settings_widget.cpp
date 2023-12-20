// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_widget.h"

#include <QtCore/QUrlQuery>
#include <QtQml/QQmlEngine>
#include <QtQuick/QQuickItem>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

AnalyticsSettingsWidget::AnalyticsSettingsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_view(std::make_unique<QQuickWidget>(appContext()->qmlEngine(), this)),
    m_store(std::make_unique<AnalyticsSettingsStore>(this))
{
    m_view->setClearColor(palette().window().color());
    m_view->setResizeMode(QQuickWidget::SizeRootObjectToView);
    m_view->setSource(QUrl("Nx/Dialogs/SystemSettings/AnalyticsSettings.qml"));
    m_view->rootObject()->setProperty("store", QVariant::fromValue(m_store.get()));

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [this]() { m_view->rootObject()->setVisible(isVisible()); });

    if (!NX_ASSERT(m_view->rootObject()))
        return;

    anchorWidgetToParent(m_view.get());

    connect(m_store.get(), &AnalyticsSettingsStore::analyticsEnginesChanged,
        this, &AnalyticsSettingsWidget::visibilityUpdateRequested);

    connect(m_store.get(), &AnalyticsSettingsStore::hasChangesChanged,
        this, &AnalyticsSettingsWidget::hasChangesChanged);
}

AnalyticsSettingsWidget::~AnalyticsSettingsWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void AnalyticsSettingsWidget::loadDataToUi()
{
    if (!m_view->rootObject())
        return;

    m_store->updateEngines();
}

void AnalyticsSettingsWidget::applyChanges()
{
    m_store->applySettingsValues();
}

void AnalyticsSettingsWidget::discardChanges()
{
    m_store->discardChanges();
}

bool AnalyticsSettingsWidget::hasChanges() const
{
    return m_store->hasChanges();
}

bool AnalyticsSettingsWidget::isNetworkRequestRunning() const
{
    return m_store->isNetworkRequestRunning();
}

bool AnalyticsSettingsWidget::activate(const QUrl& url)
{
    const QUrlQuery urlQuery(url);

    const auto& engineId = QnUuid::fromStringSafe(urlQuery.queryItemValue("engineId"));
    if (!engineId.isNull())
    {
        return QMetaObject::invokeMethod(m_view->rootObject(), "activateEngine",
            Q_ARG(QVariant, QVariant::fromValue(engineId)));
    }

    return true;
}

bool AnalyticsSettingsWidget::shouldBeVisible() const
{
    return !m_store->isEmpty();
}

} // namespace nx::vms::client::desktop
