#include "camera_info_widget.h"
#include "ui_camera_info_widget.h"
#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <ui/style/custom_style.h>
#include <ui/style/skin.h>

#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/utils/scoped_connections.h>
#include <nx/utils/log/assert.h>

namespace nx::vms::client::desktop {

namespace {

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
    ui->showOnLayoutButton->setIcon(qnSkin->icon("text_buttons/video.png"));
    ui->eventLogButton->setIcon(qnSkin->icon("text_buttons/text.png"));
    ui->cameraRulesButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));

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
        [this]() { emit actionRequested(ui::action::PingAction); });

    connect(ui->eventLogButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::CameraIssuesAction); });

    connect(ui->cameraRulesButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::CameraBusinessRulesAction); });

    connect(ui->showOnLayoutButton, &QPushButton::clicked, this,
        [this]() { emit actionRequested(ui::action::OpenInNewTabAction); });
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
    const bool singleNonWearableCamera = singleCamera
        && state.devicesDescription.isWearable == CombinedValue::None;

    ui->nameLabel->setVisible(singleCamera);

    ui->controlsStackedWidget->setCurrentWidget(singleCamera
        ? ui->toggleInfoPage
        : ui->multipleCamerasNamePage);

    ui->stackedWidget->setVisible(singleNonWearableCamera);
    ui->toggleInfoButton->setVisible(singleNonWearableCamera);
    ui->cameraRulesButton->setVisible(singleNonWearableCamera);
    ui->eventLogButton->setVisible(singleNonWearableCamera);
    ui->controlsStackedWidget->setHidden(state.isSingleWearableCamera());

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

    ui->cameraIdLabel->setText(single.id.trimmed());
    ui->cameraIdCopyButton->setHidden(ui->cameraIdLabel->text().isEmpty());

    ui->ipAddressLabel->setText(single.ipAddress);
    ui->ipAddressDetailLabel->setText(single.ipAddress);
    ui->webPageLabel->setText(single.webPage);

    const auto primaryStreamUrl = state.singleCameraSettings.primaryStream();
    ui->primaryStreamLabel->setText(primaryStreamUrl);
    ui->primaryStreamCopyButton->setHidden(primaryStreamUrl.isEmpty());

    const auto secondaryStreamUrl = state.singleCameraSettings.secondaryStream();
    ui->secondaryStreamLabel->setText(secondaryStreamUrl);
    ui->secondaryStreamCopyButton->setHidden(secondaryStreamUrl.isEmpty());

    // Hide certain fields for RTSP/HTTP links.
    const bool isNetworkLink = state.isSingleCamera()
        && state.singleCameraProperties.networkLink;

    ui->vendorTitleLabel->setHidden(isNetworkLink);
    ui->vendorLabel->setHidden(isNetworkLink);
    ui->modelTitleLabel->setHidden(isNetworkLink);
    ui->modelLabel->setHidden(isNetworkLink);
    ui->vendorDetailTitleLabel->setHidden(isNetworkLink);
    ui->vendorDetailLabel->setHidden(isNetworkLink);
    ui->modelDetailTitleLabel->setHidden(isNetworkLink);
    ui->modelDetailLabel->setHidden(isNetworkLink);
    ui->firmwareTitleLabel->setHidden(isNetworkLink);
    ui->firmwareLabel->setHidden(isNetworkLink);
    ui->macAddressTitleLabel->setHidden(isNetworkLink);
    ui->macAddressLabel->setHidden(isNetworkLink);

    const int spacerHeight = isNetworkLink ? 0 : 8;
    ui->verticalSpacer->changeSize(1, spacerHeight, QSizePolicy::Fixed, QSizePolicy::Fixed);
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
        ui->toggleInfoButton->setIcon(qnSkin->icon("text_buttons/expand.png"));
    }
    else
    {
        ui->toggleInfoButton->setText(tr("Less Info"));
        ui->toggleInfoButton->setIcon(qnSkin->icon("text_buttons/collapse.png"));
    }
}

} // namespace nx::vms::client::desktop
