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

    void updateFromResource();
    void submitToResource();

signals:
    void hasChangesChanged();

private slots:
    void at_tabWidget_currentChanged();
    void at_dataChanged();

private:
    void setHasChanges(bool hasChanges);

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget);

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_hasChanges;

    QnCameraMotionMaskWidget *m_motionWidget;
};

#endif // CAMERA_SETTINGS_DIALOG_H
