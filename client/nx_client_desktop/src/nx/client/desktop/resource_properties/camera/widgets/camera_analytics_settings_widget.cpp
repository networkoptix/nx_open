#include "camera_analytics_settings_widget.h"

#include <QtQuick/QQuickItem>
#include <QtQml/QQmlProperty>

#include "../redux/camera_settings_dialog_store.h"

namespace nx::client::desktop {

namespace {

const char* kCurrentEngineIdPropertyName = "currentEngineId";

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

    if (!NX_ASSERT(rootObject()))
        return;

    rootObject()->setProperty("store", QVariant::fromValue(store));

    QQmlProperty property(rootObject(), kCurrentEngineIdPropertyName);
    property.connectNotifySignal(this, SLOT(onCurrentEngineIdChanged()));
}

void CameraAnalyticsSettingsWidget::onCurrentEngineIdChanged()
{
    emit currentEngineIdChanged(
        rootObject()->property(kCurrentEngineIdPropertyName).value<QnUuid>());
}

} // namespace nx::client::desktop
