#pragma once

#include <QtQuickWidgets/QQuickWidget>

namespace nx::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraAnalyticsSettingsWidget: public QQuickWidget
{
    Q_OBJECT

    using base_type = QQuickWidget;

public:
    CameraAnalyticsSettingsWidget(
        CameraSettingsDialogStore* store,
        QQmlEngine* engine,
        QWidget* parent = nullptr);
};

} // namespace nx::client::desktop
