// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_info_widget.h"
#include "ui_camera_info_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scoped_connections.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/style/custom_style.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

namespace nx::vms::client::desktop {

namespace {

static const QColor kLight16Color = "#698796";
static const QColor kLight10Color = "#A5B7C0";
static const QColor kLight12Color = "#91A7B2";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}, {kLight16Color, "light16"}, {kLight12Color, "light16"}}},
    {QIcon::Active, {{kLight10Color, "light11"}, {kLight16Color, "light17"}, {kLight12Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}, {kLight12Color, "light11"}}},
};

int rowOf(QGridLayout* layout, QWidget* widget)
{
    int row{}, column{}, rowSpan{}, columnSpan{};
    layout->getItemPosition(layout->indexOf(widget), &row, &column, &rowSpan, &columnSpan);
    return row;
}

} // namespace

CameraInfoWidget::CameraInfoWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraInfoWidget())
{
    ui->setupUi(this);

    ui->multipleNameLabel->setReadOnly(true);
    ui->showOnLayoutButton->setIcon(qnSkin->icon("text_buttons/play_20.svg", kIconSubstitutions));
    ui->eventLogButton->setIcon(qnSkin->icon("text_buttons/event_log_20.svg", kIconSubstitutions));
    ui->cameraRulesButton->setIcon(qnSkin->icon("text_buttons/event_rules_20.svg", kIconSubstitutions));

    autoResizePagesToContents(ui->stackedWidget,
        {QSizePolicy::Preferred, QSizePolicy::Fixed},
        true);

    autoResizePagesToContents(ui->controlsStackedWidget,
        {QSizePolicy::MinimumExpanding, QSizePolicy::Preferred},
        true);

    alignLabels();
    updatePageSwitcher();
    updatePalette();

    connect(ui->toggleInfoButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->stackedWidget->setCurrentIndex(1 - ui->stackedWidget->currentIndex());
            updatePageSwitcher();
        });

    connect(ui->primaryStreamCopyButton, &ClipboardButton::clicked, this,
        [this]() { ClipboardButton::setClipboardText(ui->primaryStreamLabel->text()); });

    connect(ui->secondaryStreamCopyButton, &ClipboardButton::clicked, this,
        [this]() { ClipboardButton::setClipboardText(ui->secondaryStreamLabel->text()); });

    connect(ui->cameraIdCopyButton, &ClipboardButton::clicked, this,
        [this]() { ClipboardButton::setClipboardText(ui->cameraIdLabel->text()); });

    connect(ui->pingButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::PingAction); });

    connect(ui->eventLogButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::CameraIssuesAction); });

    connect(ui->cameraRulesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::CameraBusinessRulesAction); });

    connect(ui->showOnLayoutButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(menu::OpenInNewTabAction); });
}

CameraInfoWidget::~CameraInfoWidget()
{
}

void CameraInfoWidget::setStore(CameraSettingsDialogStore* store)
{
    m_storeConnections = {};
    NX_ASSERT(store);
    if (!store)
        return;

    m_storeConnections << connect(store, &CameraSettingsDialogStore::stateChanged,
        this, &CameraInfoWidget::loadState);

    m_storeConnections << connect(ui->nameLabel, &EditableLabel::textChanged,
        store, &CameraSettingsDialogStore::setSingleCameraUserName);
}

void CameraInfoWidget::loadState(const CameraSettingsDialogState& state)
{
    const bool singleCamera = state.isSingleCamera();
    const bool singleNonVirtualCamera = singleCamera
        && state.devicesDescription.isVirtualCamera == CombinedValue::None;

    ui->nameLabel->setVisible(singleCamera);

    ui->controlsStackedWidget->setCurrentWidget(singleCamera
        ? ui->toggleInfoPage
        : ui->multipleCamerasNamePage);

    ui->stackedWidget->setVisible(singleNonVirtualCamera);
    ui->toggleInfoButton->setVisible(singleNonVirtualCamera);
    ui->cameraRulesButton->setVisible(singleNonVirtualCamera && state.hasPowerUserPermissions);
    ui->eventLogButton->setVisible(singleNonVirtualCamera && state.hasEventLogPermission);

    ui->controlsStackedWidget->setHidden(state.isSingleVirtualCamera());

    const QString rulesTitle = QnCameraDeviceStringSet(
        tr("Device Rules"),tr("Camera Rules"),tr("I/O Module Rules")).getString(state.deviceType);

    ui->cameraRulesButton->setText(rulesTitle);

    const auto& single = state.singleCameraProperties;
    ui->nameLabel->setText(single.name());
    ui->nameLabel->setReadOnly(state.readOnly);
    ui->multipleNameLabel->setText(
        QnDeviceDependentStrings::getNumericName(state.deviceType, state.devicesCount));

    ui->modelLabel->setText(single.model.trimmed());
    ui->modelDetailLabel->setText(ui->modelLabel->text());
    ui->vendorLabel->setText(single.vendor.trimmed());
    ui->vendorDetailLabel->setText(ui->vendorLabel->text());
    ui->macAddressLabel->setText(single.macAddress.trimmed());
    ui->firmwareLabel->setText(single.firmware.trimmed());

    const int logicalId = state.singleCameraSettings.logicalId();
    const bool hasLogicalId = logicalId > 0;
    ui->logicalIdLabel->setText(QString::number(logicalId));
    ui->logicalIdDetailLabel->setText(ui->logicalIdLabel->text());
    ui->logicalIdTitleLabel->setVisible(hasLogicalId);
    ui->logicalIdLabel->setVisible(hasLogicalId);
    ui->logicalIdDetailTitleLabel->setVisible(hasLogicalId);
    ui->logicalIdDetailLabel->setVisible(hasLogicalId);

    ui->moreInfoLayout->setRowMinimumHeight(rowOf(ui->moreInfoLayout, ui->logicalIdDetailLabel),
        hasLogicalId ? ui->moreInfoLayout->rowMinimumHeight(0) : 0);

    ui->cameraIdLabel->setText(single.id.toSimpleString());
    ui->cameraIdCopyButton->setHidden(ui->cameraIdLabel->text().isEmpty());

    ui->ipAddressLabel->setText(single.ipAddress);
    ui->ipAddressDetailLabel->setText(single.ipAddress);
    ui->webPageLabel->setText(single.webPageLabelText);

    const auto primaryStreamUrl = state.singleCameraSettings.primaryStream();
    ui->primaryStreamLabel->setText(primaryStreamUrl);
    ui->primaryStreamCopyButton->setHidden(primaryStreamUrl.isEmpty());

    const auto secondaryStreamUrl = state.singleCameraSettings.secondaryStream();
    ui->secondaryStreamLabel->setText(secondaryStreamUrl);
    ui->secondaryStreamCopyButton->setHidden(secondaryStreamUrl.isEmpty());

    // Hide certain fields for RTSP/HTTP links.
    const bool isNetworkLink = state.isSingleCamera()
        && state.singleCameraProperties.networkLink;

    // Hide almost all fields for USB cameras.
    const bool isUsbDevice = state.isSingleCamera()
        && state.singleCameraProperties.usbDevice;

    ui->vendorTitleLabel->setHidden(isNetworkLink);
    ui->vendorLabel->setHidden(isNetworkLink);
    ui->modelTitleLabel->setHidden(isNetworkLink);
    ui->modelLabel->setHidden(isNetworkLink);
    ui->vendorDetailTitleLabel->setHidden(isNetworkLink);
    ui->vendorDetailLabel->setHidden(isNetworkLink);
    ui->modelDetailTitleLabel->setHidden(isNetworkLink);
    ui->modelDetailLabel->setHidden(isNetworkLink);
    ui->firmwareTitleLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->firmwareLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->macAddressTitleLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->macAddressLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->webPageTitleLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->webPageLabel->setHidden(isNetworkLink || isUsbDevice);
    ui->ipAddressLabel->setHidden(isUsbDevice);
    ui->ipAddressTitleLabel->setHidden(isUsbDevice);
    ui->ipAddressDetailTitleLabel->setHidden(isUsbDevice);
    ui->pingControlsWidget->setHidden(isUsbDevice);
    ui->primaryStreamTitleLabel->setHidden(isUsbDevice);
    ui->primaryStreamControlWidget->setHidden(isUsbDevice);
    ui->secondaryStreamTitleLabel->setHidden(isUsbDevice);
    ui->secondaryStreamControlWidget->setHidden(isUsbDevice);
    ui->verticalSpacer1->setHidden(isNetworkLink || (isUsbDevice && !hasLogicalId));
    ui->verticalSpacer2->setHidden(isNetworkLink);
}

void CameraInfoWidget::alignLabels()
{
    auto aligner = new Aligner(this);
    aligner->addWidgets({
        ui->vendorDetailTitleLabel,
        ui->modelDetailTitleLabel,
        ui->firmwareTitleLabel,
        ui->ipAddressDetailTitleLabel,
        ui->webPageTitleLabel,
        ui->macAddressTitleLabel,
        ui->cameraIdTitleLabel,
        ui->primaryStreamTitleLabel,
        ui->secondaryStreamTitleLabel});
}

void CameraInfoWidget::updatePalette()
{
    const std::initializer_list<QWidget*> lightBackgroundControls{
        ui->modelLabel,
        ui->modelDetailLabel,
        ui->vendorLabel,
        ui->vendorDetailLabel,
        ui->macAddressLabel,
        ui->logicalIdLabel,
        ui->logicalIdDetailLabel,
        ui->firmwareLabel,
        ui->cameraIdLabel,
        ui->ipAddressLabel,
        ui->ipAddressDetailLabel,
        ui->webPageLabel,
        ui->primaryStreamLabel,
        ui->secondaryStreamLabel};

    for (auto control: lightBackgroundControls)
        control->setForegroundRole(QPalette::Light);
}

void CameraInfoWidget::updatePageSwitcher()
{
    if (ui->stackedWidget->currentWidget() == ui->lessInfoPage)
    {
        ui->toggleInfoButton->setText(tr("More Info"));
        ui->toggleInfoButton->setIcon(
            qnSkin->icon("text_buttons/arrow_down_20.svg", kIconSubstitutions));
    }
    else
    {
        ui->toggleInfoButton->setText(tr("Less Info"));
        ui->toggleInfoButton->setIcon(
            qnSkin->icon("text_buttons/arrow_up_20.svg", kIconSubstitutions));
    }
}

} // namespace nx::vms::client::desktop
