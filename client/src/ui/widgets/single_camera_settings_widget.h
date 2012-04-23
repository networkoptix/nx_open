#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"

namespace Ui {
    class SingleCameraSettingsWidget;
}

class QnCameraMotionMaskWidget;

class QnSingleCameraSettingsWidget : public QWidget {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

public:
    explicit QnSingleCameraSettingsWidget(QWidget *parent = NULL);
    virtual ~QnSingleCameraSettingsWidget();

    const QnVirtualCameraResourcePtr &camera() const;
    void setCamera(const QnVirtualCameraResourcePtr &camera);

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    bool isCameraActive() const;
    void setCameraActive(bool active);

    bool hasChanges() const {
        return m_hasChanges;
    }

    bool isReadOnly() const;
    void setReadOnly(bool readOnly);

    void updateFromResource();
    void submitToResource();

signals:
    void hasChangesChanged();
    void moreLicensesRequested();

private slots:
    void at_tabWidget_currentChanged();
    void at_dataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_motionSensitivityChanged(int value);
private:
    void setHasChanges(bool hasChanges);

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget);

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_hasChanges;
    bool m_hasScheduleChanges;
    bool m_readOnly;

    QnCameraMotionMaskWidget *m_motionWidget;
};

#endif // CAMERA_SETTINGS_DIALOG_H
