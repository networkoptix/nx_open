// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_analytics_settings_widget.h"

#include <QtQuick/QQuickItem>

#include <nx/vms/client/desktop/analytics/analytics_actions_helper.h>
#include <nx/vms/client/desktop/common/utils/engine_license_summary_provider.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <utils/common/event_processors.h>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kMinimumSize(800, 400);

} // namespace

CameraAnalyticsSettingsWidget::CameraAnalyticsSettingsWidget(
    SystemContext* context,
    CameraSettingsDialogStore* store,
    QQmlEngine* engine,
    QWidget* parent)
    :
    base_type(engine, parent),
    SystemContextAware(context)
{
    setClearColor(palette().window().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl("Nx/Dialogs/CameraSettings/AnalyticsSettings.qml"));
    setMinimumSize(kMinimumSize);

    if (!NX_ASSERT(rootObject()))
        return;

    installEventHandler(this, {QEvent::Show, QEvent::Hide}, this,
        [this]() { rootObject()->setVisible(isVisible()); });

    rootObject()->setVisible(false);

    rootObject()->setProperty("store", QVariant::fromValue(store));
    rootObject()->setProperty("backend", QVariant::fromValue(this));

    const auto engineLicenseSummaryProvider =
        new EngineLicenseSummaryProvider{systemContext(), this};
    rootObject()->setProperty(
        "engineLicenseSummaryProvider", QVariant::fromValue(engineLicenseSummaryProvider));

    setHelpTopic(this, Qn::PluginsAndAnalytics_Help);
}

QVariant CameraAnalyticsSettingsWidget::requestParameters(const QJsonObject& model)
{
    auto result = AnalyticsActionsHelper::requestSettingsJson(model, this);
    return result ? *result : QVariant{};
}

} // namespace nx::vms::client::desktop
