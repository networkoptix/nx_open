#pragma once

#include <QtQuickWidgets/QQuickWidget>

#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

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

signals:
    void currentEngineIdChanged(const QnUuid& engineId);

private slots:
    void onCurrentEngineIdChanged();
};

} // namespace nx::vms::client::desktop
