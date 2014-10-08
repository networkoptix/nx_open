#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/dialogs/workbench_state_dependent_dialog.h>

class QAbstractButton;

class QnCameraSettingsWidget;

class QnCameraSettingsDialog: public QnWorkbenchStateDependentButtonBoxDialog {
    Q_OBJECT

    typedef QnWorkbenchStateDependentButtonBoxDialog base_type;
public:
    QnCameraSettingsDialog(QWidget *parent = NULL);
    virtual ~QnCameraSettingsDialog();

    virtual bool tryClose(bool force) override;

    void setCameras(const QnVirtualCameraResourceList &cameras, bool force = false);

    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;

signals:
    void cameraOpenRequested();

private slots:
    void at_settingsWidget_hasChangesChanged();
    void at_settingsWidget_modeChanged();
    void at_advancedSettingChanged();
    void at_selectionChangeAction_triggered();

    void at_diagnoseButton_clicked();
    void at_rulesButton_clicked();
    void at_openButton_clicked();

    void at_cameras_saved(ec2::ErrorCode errorCode, const QnVirtualCameraResourceList &cameras);
    void at_cameras_properties_saved(int requestId, ec2::ErrorCode errorCode);
    void at_camera_settings_saved(int httpStatusCode, const QList<QPair<QString, bool> >& operationResult);

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
