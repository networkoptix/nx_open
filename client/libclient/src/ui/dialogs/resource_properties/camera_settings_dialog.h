#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/dialogs/common/workbench_state_dependent_dialog.h>

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

    Qn::CameraSettingsTab currentTab() const;
    void setCurrentTab(Qn::CameraSettingsTab tab);

    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;
    virtual void retranslateUi() override;

private slots:
    void at_settingsWidget_hasChangesChanged();
    void at_settingsWidget_modeChanged();

    void at_diagnoseButton_clicked();
    void at_rulesButton_clicked();
    void at_openButton_clicked();

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

private:
    QnCameraSettingsWidget *m_settingsWidget;
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_applyButton, *m_okButton, *m_openButton, *m_diagnoseButton, *m_rulesButton;

    bool m_ignoreAccept;
};


#endif // QN_CAMERA_SETTINGS_DIALOG_H
