#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/uuid.h>

namespace nx::client::desktop {

struct CameraSettingsDialogState;
class CameraSettingsDialogStore;

class CameraAnalyticsSettingsWidget: public QQuickWidget
{
    using base_type = QQuickWidget;

public:
    CameraAnalyticsSettingsWidget(
        CameraSettingsDialogStore* store,
        QQmlEngine* engine,
        QWidget* parent = nullptr);
};

} // namespace nx::client::desktop
