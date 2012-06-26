#ifndef QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
#define QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"

namespace Ui {
    class MultipleCameraSettingsWidget;
}

class QnMultipleCameraSettingsWidget : public QWidget {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

public:
    explicit QnMultipleCameraSettingsWidget(QWidget *parent);
    ~QnMultipleCameraSettingsWidget();

    const QnVirtualCameraResourceList &cameras() const;
    void setCameras(const QnVirtualCameraResourceList &cameras);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    int activeCameraCount() const;
    void setCamerasActive(bool active);

    void updateFromResources();
    void submitToResources();

    bool hasChanges() const {
        return m_hasChanges;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

signals:
    void hasChangesChanged();
    void moreLicensesRequested();

private slots:
    void at_dataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_cameraScheduleWidget_scheduleEnabledChanged();
    void at_enableAudioCheckBox_clicked();
    void updateMaxFPS();
private:
    void setHasChanges(bool hasChanges);

private:
    Q_DISABLE_COPY(QnMultipleCameraSettingsWidget);

    QScopedPointer<Ui::MultipleCameraSettingsWidget> ui;
    QnVirtualCameraResourceList m_cameras;
    bool m_hasChanges;
    bool m_hasScheduleChanges;
    bool m_hasScheduleEnabledChanges;
    bool m_readOnly;
    bool m_inUpdateMaxFps;
};

#endif // QN_MULTIPLE_CAMERA_SETTINGS_DIALOG_H
