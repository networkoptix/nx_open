#pragma once

#include <QtWidgets/QDialogButtonBox>

#include <core/resource/resource_fwd.h>
#include <ui/dialogs/common/session_aware_dialog.h>

#include <nx/vms/client/desktop/resource_properties/camera/camera_settings_tab.h>

class QAbstractButton;

namespace nx::vms::client::desktop {

class CameraSettingsWidget;
class DefaultPasswordAlertBar;

class LegacyCameraSettingsDialog: public QnSessionAwareButtonBoxDialog
{
    Q_OBJECT

    using base_type = QnSessionAwareButtonBoxDialog;

public:
    LegacyCameraSettingsDialog(QWidget* parent);
    virtual ~LegacyCameraSettingsDialog();

    virtual bool tryClose(bool force) override;

    bool setCameras(const QnVirtualCameraResourceList &cameras, bool force = false);

    void submitToResources();
    void updateFromResources();

    CameraSettingsTab currentTab() const;
    void setCurrentTab(CameraSettingsTab tab);

    virtual void accept() override;
    virtual void reject() override;

protected:
    virtual void buttonBoxClicked(QDialogButtonBox::StandardButton button) override;
    virtual void retranslateUi() override;

private slots:
    void at_settingsWidget_hasChangesChanged();
    void updateButtonsVisibility();

    void at_diagnoseButton_clicked();
    void at_rulesButton_clicked();
    void at_openButton_clicked();

private:
    void updateReadOnly();
    void saveCameras(const QnVirtualCameraResourceList &cameras);
    void setupDefaultPasswordListener();
    void handleCamerasWithDefaultPasswordChanged();
    void handleChangeDefaultPasswordRequest();

private:
    CameraSettingsWidget* m_settingsWidget;
    QDialogButtonBox* m_buttonBox;
    QPushButton *m_applyButton, *m_okButton, *m_openButton, *m_diagnoseButton, *m_rulesButton;
    DefaultPasswordAlertBar* m_defaultPasswordAlert;
    bool m_ignoreAccept;
};

} // namespace nx::vms::client::desktop
