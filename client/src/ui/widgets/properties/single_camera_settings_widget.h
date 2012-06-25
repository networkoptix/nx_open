#ifndef CAMERA_SETTINGS_DIALOG_H
#define CAMERA_SETTINGS_DIALOG_H

#include <QtGui/QWidget>
#include <core/resource/resource_fwd.h>
#include "camera_settings_tab.h"

namespace Ui {
    class SingleCameraSettingsWidget;
}

class QVBoxLayout;

class QnCameraMotionMaskWidget;

class QnSingleCameraSettingsWidget : public QWidget {
    Q_OBJECT;
    Q_PROPERTY(bool readOnly READ isReadOnly WRITE setReadOnly);

    typedef QWidget base_type;

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

    /** Check if motion region is valid */
    bool isValidMotionRegion();

signals:
    void hasChangesChanged();
    void moreLicensesRequested();

protected:
    virtual void showEvent(QShowEvent *event) override;
    virtual void hideEvent(QHideEvent *event) override;

private slots:
    void at_tabWidget_currentChanged();
    void at_dataChanged();
    void at_cameraScheduleWidget_scheduleTasksChanged();
    void at_linkActivated(const QString &urlString);
    void at_motionTypeChanged();
    void at_motionSelectionCleared();
    
    void updateMaxFPS();
    void updateMotionWidgetSensitivity();

private:
    void setHasChanges(bool hasChanges);

    void updateMotionWidgetFromResource();
    void submitMotionWidgetToResource();

    void updateMotionWidgetNeedControlMaxRect();

    void disconnectFromMotionWidget();
    void connectToMotionWidget();

private:
    Q_DISABLE_COPY(QnSingleCameraSettingsWidget);

    QScopedPointer<Ui::SingleCameraSettingsWidget> ui;
    QnVirtualCameraResourcePtr m_camera;
    bool m_cameraSupportsMotion;

    bool m_hasChanges;
    bool m_hasScheduleChanges;
    bool m_readOnly;

    QnCameraMotionMaskWidget *m_motionWidget;
    QVBoxLayout *m_motionLayout;
    bool m_inUpdateMaxFps;
};

#endif // CAMERA_SETTINGS_DIALOG_H
