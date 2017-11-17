#ifndef QN_CAMERA_SETTINGS_DIALOG_H
#define QN_CAMERA_SETTINGS_DIALOG_H

#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>

#include <ui/widgets/properties/camera_settings_widget.h>

#include <ui/dialogs/common/session_aware_dialog.h>

class QAbstractButton;

class QnCameraSettingsWidget;
class QnDefaultPasswordAlertBar;

class QnCameraSettingsDialog: public QnSessionAwareButtonBoxDialog {
    Q_OBJECT

    typedef QnSessionAwareButtonBoxDialog base_type;
public:
    QnCameraSettingsDialog(QWidget *parent);
    virtual ~QnCameraSettingsDialog();

    virtual bool tryClose(bool force) override;

    bool setCameras(const QnVirtualCameraResourceList &cameras, bool force = false);

    void submitToResources();
    void updateFromResources();

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
    void saveCameras(const QnVirtualCameraResourceList &cameras);
    void handleCamerasWithDefaultPasswordChanged();
    void handleChangeDefaultPasswordRequest(bool showSingleCamera);

private:
    QnCameraSettingsWidget *m_settingsWidget;
    QDialogButtonBox *m_buttonBox;
    QPushButton *m_applyButton, *m_okButton, *m_openButton, *m_diagnoseButton, *m_rulesButton;
    QnDefaultPasswordAlertBar* m_defaultPasswordAlert;
    bool m_ignoreAccept;
};


#endif // QN_CAMERA_SETTINGS_DIALOG_H
