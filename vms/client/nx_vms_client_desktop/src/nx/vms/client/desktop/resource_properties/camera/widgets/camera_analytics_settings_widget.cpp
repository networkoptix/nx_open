#include "camera_analytics_settings_widget.h"

#include <QtQuick/QQuickItem>

#include "../redux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kMinimumHeight = 400;

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
    setMinimumHeight(kMinimumHeight);

    if (!NX_ASSERT(rootObject()))
        return;

    rootObject()->setProperty("store", QVariant::fromValue(store));
}

} // namespace nx::vms::client::desktop
