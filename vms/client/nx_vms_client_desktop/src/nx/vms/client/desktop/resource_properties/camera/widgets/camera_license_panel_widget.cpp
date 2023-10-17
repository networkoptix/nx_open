// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_license_panel_widget.h"
#include "ui_camera_license_panel_widget.h"

#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/desktop/common/utils/background_flasher.h>
#include <nx/vms/client/desktop/common/utils/check_box_utils.h>
#include <nx/vms/client/desktop/common/utils/provided_text_display.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/read_only.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"
#include "../utils/license_usage_provider.h"

namespace nx::vms::client::desktop {

CameraLicensePanelWidget::CameraLicensePanelWidget(QWidget *parent):
    base_type(parent),
    ui(new Ui::CameraLicensePanelWidget),
    m_licenseUsageDisplay(new ProvidedTextDisplay())
{
    ui->setupUi(this);

    check_box_utils::autoClearTristate(ui->useLicenseCheckBox);
    ui->useLicenseCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->useLicenseCheckBox->setForegroundRole(QPalette::ButtonText);

    ui->licenseUsageLabel->setMinimumHeight(style::Metrics::kButtonHeight);
    ui->licenseUsageLabel->setAutoFillBackground(true);
    ui->licenseUsageLabel->setContentsMargins({
        style::Metrics::kStandardPadding, 0, style::Metrics::kStandardPadding, 0 });

    m_licenseUsageDisplay->setDisplayingWidget(ui->licenseUsageLabel);

    connect(ui->moreLicensesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::PreferencesLicensesTabAction); });
}

CameraLicensePanelWidget::~CameraLicensePanelWidget()
{
}

void CameraLicensePanelWidget::init(
    LicenseUsageProvider* licenseUsageProvider, CameraSettingsDialogStore* store)
{
    m_licenseUsageProvider = licenseUsageProvider;
    m_licenseUsageDisplay->setTextProvider(licenseUsageProvider);
    m_storeConnections = {};

    m_storeConnections << connect(ui->useLicenseCheckBox, &QCheckBox::clicked,
        store, &CameraSettingsDialogStore::setRecordingEnabled);

    m_storeConnections << connect(ui->useLicenseCheckBox, &CheckBox::cannotBeToggled, this,
        [this]()
        {
            BackgroundFlasher::flash(ui->licenseUsageLabel, core::colorTheme()->color("red_l2"));
        });

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraLicensePanelWidget::loadState);

    m_storeConnections << connect(licenseUsageProvider, &LicenseUsageProvider::stateChanged, store,
        [this, store]()
        {
            ui->useLicenseCheckBox->setCanBeChecked(!m_licenseUsageProvider->limitExceeded());
            updateLicensesButton(store->state());
        });
}

void CameraLicensePanelWidget::loadState(const CameraSettingsDialogState& state)
{
    check_box_utils::setupTristateCheckbox(ui->useLicenseCheckBox, state.recording.enabled);
    updateLicensesButton(state);
    ui->useLicenseCheckBox->setText(tr("Use License", "", state.devicesCount));
    setReadOnly(ui->useLicenseCheckBox, state.readOnly);
}

void CameraLicensePanelWidget::updateLicensesButton(const CameraSettingsDialogState& state)
{
    ui->moreLicensesButton->setVisible(state.hasPowerUserPermissions
        && m_licenseUsageProvider && m_licenseUsageProvider->limitExceeded());
}

} // namespace nx::vms::client::desktop
