#ifndef QN_CAMERA_SETTINGS_WIDGET_H
#define QN_CAMERA_SETTINGS_WIDGET_H

#include <QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"

class QStackedWidget;

class QnSingleCameraSettingsWidget;
class QnMultipleCameraSettingsWidget;

class QnCameraSettingsWidget: public QWidget {
public:
    enum Mode {
        EmptyMode,
        SingleMode,
        MultiMode
    };

    QnCameraSettingsWidget(QWidget *parent);
    virtual ~QnCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    bool hasChanges() const;

    void updateFromResources();
    void submitToResources();

    Mode mode() const;

signals:
    void hasChangesChanged();

private:
    void setMode(Mode mode);

private:
    QnVirtualCameraResourceList m_cameras;
    Qn::CameraSettingsTab m_emptyTab;

    QStackedWidget *m_stackedWidget;
    QWidget *m_emptyWidget;
    QnSingleCameraSettingsWidget *m_singleWidget;
    QnMultipleCameraSettingsWidget *m_multiWidget;
};


#endif // QN_CAMERA_SETTINGS_WIDGET_H
