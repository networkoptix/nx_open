// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_analytics_settings_widget.h"

#include <QtQuick/QQuickItem>

#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr QSize kMinimumSize(800, 400);

} // namespace

CameraAnalyticsSettingsWidget::CameraAnalyticsSettingsWidget(
    CameraSettingsDialogStore* store,
    QQmlEngine* engine,
    QWidget* parent)
    :
    base_type(engine, parent)
{
    setClearColor(palette().window().color());
    setResizeMode(QQuickWidget::SizeRootObjectToView);
    setSource(QUrl("Nx/Dialogs/CameraSettings/AnalyticsSettings.qml"));
    setMinimumSize(kMinimumSize);

    if (!NX_ASSERT(rootObject()))
        return;

    rootObject()->setProperty("store", QVariant::fromValue(store));
}

} // namespace nx::vms::client::desktop
