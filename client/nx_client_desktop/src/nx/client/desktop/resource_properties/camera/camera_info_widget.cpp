#include "camera_info_widget.h"
#include "ui_camera_info_widget.h"

#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>

#include <nx/utils/log/assert.h>

#include <ui/style/custom_style.h>
#include <ui/style/skin.h>
#include <ui/common/aligner.h>

#include "camera_settings_model.h"

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

    connect(ui->nameLabel, &QnEditableLabel::textChanged, this,
        &CameraInfoWidget::hasChangesChanged);
}

CameraInfoWidget::~CameraInfoWidget()
{
}

void CameraInfoWidget::setModel(CameraSettingsModel* model)
{
    NX_EXPECT(model && !m_model);

    if (m_model)
    {
        m_model->disconnect(this);
        ui->pingButton->disconnect(m_model);
        ui->showOnLayoutButton->disconnect(m_model);
        ui->eventLogButton->disconnect(m_model);
        ui->cameraRulesButton->disconnect(m_model);
    }

    m_model = model;

    if (m_model)
    {
        connect(m_model, &CameraSettingsModel::networkInfoChanged, this,
            &CameraInfoWidget::updateNetworkInfo);

        connect(ui->pingButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::pingCamera);
        connect(ui->showOnLayoutButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::showCamerasOnLayout);
        connect(ui->eventLogButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::openEventLog);
        connect(ui->cameraRulesButton, &QPushButton::clicked, m_model,
            &CameraSettingsModel::openCameraRules);
    }
}

bool CameraInfoWidget::hasChanges() const
{
    NX_EXPECT(m_model);
    if (!m_model || !m_model->isSingleCameraMode())
        return false;

    return m_model->name() != ui->nameLabel->text();
}

void CameraInfoWidget::loadDataToUi()
{
    NX_EXPECT(m_model);
    if (!m_model)
        return;

    const bool singleCamera = m_model->isSingleCameraMode();
    ui->nameLabel->setVisible(singleCamera);
    ui->controlsStackedWidget->setCurrentWidget(singleCamera
        ? ui->toggleInfoPage
        : ui->multipleCamerasNamePage);

    ui->stackedWidget->setVisible(singleCamera);
    ui->cameraRulesButton->setVisible(singleCamera);
    if (singleCamera)
    {
        const auto camera = m_model->singleCamera();
        NX_EXPECT(camera);
        const QString rulesTitle = QnDeviceDependentStrings::getNameFromSet(
            camera->resourcePool(),
            QnCameraDeviceStringSet(
                tr("Device Rules..."),
                tr("Camera Rules..."),
                tr("I/O Module Rules...")
            ), camera);

        ui->cameraRulesButton->setText(rulesTitle);
    }

    ui->nameLabel->setText(m_model->name());
    ui->multipleNameLabel->setText(m_model->name());

    ui->modelLabel->setText(m_model->info().model);
    ui->modelDetailLabel->setText(m_model->info().model);
    ui->vendorLabel->setText(m_model->info().vendor);
    ui->vendorDetailLabel->setText(m_model->info().vendor);
    ui->macAddressLabel->setText(m_model->info().macAddress);
    ui->firmwareLabel->setText(m_model->info().firmware);
    ui->cameraIdLabel->setText(m_model->info().id);

    updateNetworkInfo();
}

void CameraInfoWidget::applyChanges()
{
    NX_EXPECT(m_model);
    if (!m_model)
        return;

    if (m_model->isSingleCameraMode())
        m_model->setName(ui->nameLabel->text());
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

void CameraInfoWidget::updateNetworkInfo()
{
    NX_EXPECT(m_model);
    ui->ipAddressLabel->setText(m_model->networkInfo().ipAddress);
    ui->ipAddressDetailLabel->setText(m_model->networkInfo().ipAddress);
    ui->webPageLabel->setText(m_model->networkInfo().webPage);
    ui->primaryStreamLabel->setText(m_model->networkInfo().primaryStream);
    ui->secondaryStreamLabel->setText(m_model->networkInfo().secondaryStream);
}

} // namespace desktop
} // namespace client
} // namespace nx
