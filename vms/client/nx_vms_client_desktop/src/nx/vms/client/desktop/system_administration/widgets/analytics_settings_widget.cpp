// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_widget.h"

#include <QtCore/QUrlQuery>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

#include "private/analytics_settings_widget_p.h"

using namespace nx::vms::common;

namespace nx::vms::client::desktop {

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
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

void AnalyticsSettingsWidget::loadDataToUi()
{
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
    if (auto api = d->connectedServerApi())
    {
        for (auto handle: d->pendingApplyRequests)
            api->cancelRequest(handle);
        for (auto handle: d->pendingRefreshRequests)
            api->cancelRequest(handle);
    }
    d->pendingApplyRequests.clear();
    d->pendingRefreshRequests.clear();
}

bool AnalyticsSettingsWidget::hasChanges() const
{
    return d->hasChanges;
}

bool AnalyticsSettingsWidget::isNetworkRequestRunning() const
{
    return !d->pendingApplyRequests.empty() || !d->pendingRefreshRequests.empty();
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
