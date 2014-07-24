#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/workbench/workbench_context_aware.h>

class QAbstractButton;

class QnCameraSettingsWidget;
class QnWorkbenchStateDelegate;

class QnCameraSettingsDialog: public QDialog, public QnWorkbenchContextAware {
    Q_OBJECT
public:
    QnCameraSettingsDialog(QWidget *parent = NULL);
    virtual ~QnCameraSettingsDialog();

    bool tryClose(bool force);

    void setCameras(const QnVirtualCameraResourceList &cameras, bool force = false);
signals:
    void cameraOpenRequested();

private slots:
    void at_buttonBox_clicked(QAbstractButton *button);
    void at_settingsWidget_hasChangesChanged();
    void at_settingsWidget_modeChanged();
    void at_advancedSettingChanged();
    void at_selectionChangeAction_triggered();

    void at_diagnoseButton_clicked();
    void at_rulesButton_clicked();
    void at_openButton_clicked();

    void at_cameras_saved(ec2::ErrorCode errorCode, const QnVirtualCameraResourceList &cameras);
    void at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult);

    void acceptIfSafe();
    void updateCamerasFromSelection();
private:
    void updateReadOnly();

    /**
     * Save modified camera settings to server.
     * \param checkControls - if set then additional check will occur.
     * If user modified some of control elements but did not apply changes he will be asked to fix it.
     * \see Feature #1195
     */
    void submitToResources(bool checkControls = false);
    
    void saveCameras(const QnVirtualCameraResourceList &cameras);

    void saveAdvancedCameraSettingsAsync(const QnVirtualCameraResourceList &cameras);
private:
    QScopedPointer<QnWorkbenchStateDelegate> m_workbenchStateDelegate;

    QnCameraSettingsWidget *m_settingsWidget;
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_applyButton, *m_okButton, *m_openButton, *m_diagnoseButton, *m_rulesButton;

    /** Whether the set of selected resources was changed and settings
     * dialog is waiting to be updated. */
    bool m_selectionUpdatePending;

    /** Scope of the last selection change. */
    Qn::ActionScope m_selectionScope;

    bool m_ignoreAccept;
};


#endif // QN_CAMERA_SETTINGS_DIALOG_H
