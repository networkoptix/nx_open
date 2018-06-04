#include "camera_license_panel_widget.h"
#include "ui_camera_license_panel_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <ui/common/read_only.h>
#include <ui/style/helper.h>

#include <nx/client/desktop/common/utils/provided_text_display.h>
#include <nx/client/desktop/common/utils/checkbox_utils.h>
#include <nx/utils/disconnect_helper.h>

namespace nx {
namespace client {
namespace desktop {

CameraLicensePanelWidget::CameraLicensePanelWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraLicensePanelWidget),
    m_licenseUsageDisplay(new ProvidedTextDisplay())
{
    ui->setupUi(this);

    CheckboxUtils::autoClearTristate(ui->useLicenseCheckBox);
    ui->useLicenseCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->useLicenseCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->licenseUsageLabel->setMinimumHeight(style::Metrics::kButtonHeight);
    m_licenseUsageDisplay->setDisplayingWidget(ui->licenseUsageLabel);

    connect(ui->moreLicensesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::PreferencesLicensesTabAction); });
}

CameraLicensePanelWidget::~CameraLicensePanelWidget()
{
}

void CameraLicensePanelWidget::init(
    AbstractTextProvider* licenseUsageTextProvider, CameraSettingsDialogStore* store)
{
    m_licenseUsageDisplay->setTextProvider(licenseUsageTextProvider);
    m_storeConnections.reset(new QnDisconnectHelper());

    *m_storeConnections << connect(ui->useLicenseCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRecordingEnabled);

    *m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraLicensePanelWidget::loadState);
}

void CameraLicensePanelWidget::loadState(const CameraSettingsDialogState& state)
{
    CheckboxUtils::setupTristateCheckbox(ui->useLicenseCheckBox, state.recording.enabled);

    ui->moreLicensesButton->setVisible(state.globalPermissions.testFlag(Qn::GlobalAdminPermission));

    ui->useLicenseCheckBox->setText(tr("Use License", "", state.devicesCount));
    setReadOnly(ui->useLicenseCheckBox, state.readOnly);
}

} // namespace desktop
} // namespace client
} // namespace nx
