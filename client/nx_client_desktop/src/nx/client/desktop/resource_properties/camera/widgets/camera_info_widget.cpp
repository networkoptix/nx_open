#include "camera_info_widget.h"
#include "ui_camera_info_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <nx/utils/log/assert.h>

#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/common/aligner.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

namespace nx {
namespace client {
namespace desktop {

CameraInfoWidget::CameraInfoWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::CameraInfoWidget())
{
    ui->setupUi(this);

    ui->multipleNameLabel->setReadOnly(true);
    ui->showOnLayoutButton->setIcon(qnSkin->icon("text_buttons/video.png"));
    ui->eventLogButton->setIcon(qnSkin->icon("buttons/event_log.png"));
    ui->cameraRulesButton->setIcon(qnSkin->icon("text_buttons/event_rules.png"));

    autoResizePagesToContents(ui->stackedWidget,
        { QSizePolicy::Preferred, QSizePolicy::Fixed },
        true);

    autoResizePagesToContents(ui->controlsStackedWidget,
        { QSizePolicy::Fixed, QSizePolicy::Preferred},
        true);

    alignLabels();
    updatePageSwitcher();
    updatePalette();

    connect(ui->toggleInfoButton, &QPushButton::clicked, this,
        [this]
        {
            ui->stackedWidget->setCurrentIndex(1 - ui->stackedWidget->currentIndex());
            updatePageSwitcher();
        });

    connect(ui->primaryStreamCopyButton, &ui::ClipboardButton::clicked, this,
        [this] { ui::ClipboardButton::setClipboardText(ui->primaryStreamLabel->text()); });
    connect(ui->secondaryStreamCopyButton, &ui::ClipboardButton::clicked, this,
        [this] { ui::ClipboardButton::setClipboardText(ui->secondaryStreamLabel->text()); });




    /*
            connect(ui->pingButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::pingCamera);
        connect(ui->showOnLayoutButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::showCamerasOnLayout);
        connect(ui->eventLogButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::openEventLog);
        connect(ui->cameraRulesButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::openCameraRules);
            void CameraSettingsModel::pingCamera()
{
    menu()->trigger(ui::action::PingAction, {Qn::TextRole, d->networkInfo.ipAddress});
}

void CameraSettingsModel::showCamerasOnLayout()
{
    menu()->trigger(ui::action::OpenInNewTabAction, d->adaptor.cameras());
}

void CameraSettingsModel::openEventLog()
{
    menu()->trigger(ui::action::CameraIssuesAction, d->adaptor.cameras());
}

void CameraSettingsModel::openCameraRules()
{
    menu()->trigger(ui::action::CameraBusinessRulesAction, d->adaptor.cameras());
}

    */
}

CameraInfoWidget::~CameraInfoWidget() = default;

void CameraInfoWidget::setStore(CameraSettingsDialogStore* store)
{
    connect(store, &CameraSettingsDialogStore::stateChanged, this, &CameraInfoWidget::loadState);

    connect(ui->nameLabel, &QnEditableLabel::textChanged, store,
        [store](const QString& text)
        {
            store->setSingleCameraUserName(text);
        });
}

void CameraInfoWidget::loadState(const CameraSettingsDialogState& state)
{
    const bool singleCamera = state.isSingleCamera();
    ui->nameLabel->setVisible(singleCamera);
    ui->controlsStackedWidget->setCurrentWidget(singleCamera
        ? ui->toggleInfoPage
        : ui->multipleCamerasNamePage);

    ui->stackedWidget->setVisible(singleCamera);
    ui->cameraRulesButton->setVisible(singleCamera);

    const QString rulesTitle = QnCameraDeviceStringSet(
        tr("Device Rules..."),
        tr("Camera Rules..."),
        tr("I/O Module Rules...")
    ).getString(state.deviceType);

    ui->cameraRulesButton->setText(rulesTitle);

    ui->nameLabel->setText(state.singleCameraSettings.name());
    ui->nameLabel->setReadOnly(state.readOnly);
    ui->multipleNameLabel->setText(
        QnDeviceDependentStrings::getNumericName(state.deviceType, state.devicesCount));

    ui->modelLabel->setText(state.singleCameraSettings.model);
    ui->modelDetailLabel->setText(state.singleCameraSettings.model);
    ui->vendorLabel->setText(state.singleCameraSettings.vendor);
    ui->vendorDetailLabel->setText(state.singleCameraSettings.vendor);
    ui->macAddressLabel->setText(state.singleCameraSettings.macAddress);
    ui->firmwareLabel->setText(state.singleCameraSettings.firmware);
    ui->cameraIdLabel->setText(state.singleCameraSettings.id);

    ui->ipAddressLabel->setText(state.singleCameraSettings.ipAddress);
    ui->ipAddressDetailLabel->setText(state.singleCameraSettings.ipAddress);
    ui->webPageLabel->setText(state.singleCameraSettings.webPage);
    ui->primaryStreamLabel->setText(state.singleCameraSettings.primaryStream);
    ui->secondaryStreamLabel->setText(state.singleCameraSettings.secondaryStream);
}

void CameraInfoWidget::alignLabels()
{
    auto aligner = new QnAligner(this);
    aligner->addWidgets({
        ui->vendorDetailTitleLabel,
        ui->modelDetailTitleLabel,
        ui->firmwareTitleLabel,
        ui->ipAddressDetailTitleLabel,
        ui->webPageTitleLabel,
        ui->macAddressTitleLabel,
        ui->cameraIdTitleLabel,
        ui->primaryStreamTitleLabel,
        ui->secondaryStreamTitleLabel
    });
}

void CameraInfoWidget::updatePalette()
{
    for (auto control: {
        ui->modelLabel,
        ui->modelDetailLabel,
        ui->vendorLabel,
        ui->vendorDetailLabel,
        ui->macAddressLabel,
        ui->firmwareLabel,
        ui->cameraIdLabel,
        ui->ipAddressLabel,
        ui->ipAddressDetailLabel,
        ui->webPageLabel,
        ui->primaryStreamLabel,
        ui->secondaryStreamLabel
    })
    {
        control->setForegroundRole(QPalette::Light);
    }
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

} // namespace desktop
} // namespace client
} // namespace nx
