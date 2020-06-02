#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"
#include "fisheye_calibration_widget.h"

#include <QtCore/QSignalBlocker>

#include <client/client_settings.h>
#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/event_processors.h>

#include <nx/vms/client/desktop/common/utils/widget_anchor.h>

namespace nx::vms::client::desktop {

FisheyeSettingsWidget::FisheyeSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FisheyeSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::CameraSettings_Dewarping_Help);

    ui->angleLabel->setHint(tr("Use this setting to compensate for distortion if camera "
        "is not mounted exactly vertically or horizontally."));

    setHelpTopic(ui->angleLabel, Qn::CameraSettings_Dewarping_Help);

    ui->sizeIcon1->setPixmap(qnSkin->pixmap("fisheye/circle_small.png"));
    ui->sizeIcon2->setPixmap(qnSkin->pixmap("fisheye/circle_big.png"));
    ui->xOffsetIcon1->setPixmap(qnSkin->pixmap("fisheye/arrow_left.png"));
    ui->xOffsetIcon2->setPixmap(qnSkin->pixmap("fisheye/arrow_right.png"));
    ui->yOffsetIcon1->setPixmap(qnSkin->pixmap("fisheye/arrow_down.png"));
    ui->yOffsetIcon2->setPixmap(qnSkin->pixmap("fisheye/arrow_up.png"));
    ui->ellipticityIcon1->setPixmap(qnSkin->pixmap("fisheye/ellipse_vertical.png"));
    ui->ellipticityIcon2->setPixmap(qnSkin->pixmap("fisheye/ellipse_horizontal.png"));

    connect(ui->angleSpinBox, QnDoubleSpinBoxValueChanged,
        this, &FisheyeSettingsWidget::dataChanged);

    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::dataChanged,
        this, &FisheyeSettingsWidget::dataChanged);

    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, ui->calibrateWidget,
        &FisheyeCalibrationWidget::setEnabled);

    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, ui->settingsWidget,
        &FisheyeSettingsWidget::setEnabled);

    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, this,
        &FisheyeSettingsWidget::dataChanged);

    connect(ui->sizeSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->sizeSlider);
            ui->calibrateWidget->setRadius(value / 1000.0);
        });

    connect(ui->xOffsetSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->xOffsetSlider);
            ui->calibrateWidget->setCenter(
                QPointF(value / 1000.0, ui->calibrateWidget->center().y()));
        });

    connect(ui->yOffsetSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->yOffsetSlider);
            ui->calibrateWidget->setCenter(
                QPointF(ui->calibrateWidget->center().x(), 1.0 - value / 1000.0));
        });

    connect(ui->ellipticitySlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->ellipticitySlider);
            ui->calibrateWidget->setHorizontalStretch(value / 500.0);
        });

    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::dataChanged, this,
        [this]()
        {
            ui->sizeSlider->setValue(int(ui->calibrateWidget->radius() * 1000.0));
            ui->ellipticitySlider->setValue(int(ui->calibrateWidget->horizontalStretch() * 500.0));
            ui->xOffsetSlider->setValue(int(ui->calibrateWidget->center().x() * 1000.0));
            ui->yOffsetSlider->setValue(int((1.0 - ui->calibrateWidget->center().y()) * 1000.0));
            emit dataChanged();
        });

    connect(ui->autoCalibrationButton, &QPushButton::clicked, this,
        [this]()
        {
            ui->autoCalibrationButton->setEnabled(false);
            ui->calibrateWidget->autoCalibrate();
        });

    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::autoCalibrationFinished, this,
        [this]()
        {
            ui->autoCalibrationButton->setEnabled(true);
        });

    connect(ui->showGridCheckBox, &QCheckBox::toggled, this,
        [this](bool checked)
        {
            qnSettings->setFisheyeCalibrationGridShown(checked);
            ui->calibrateWidget->setGridShown(checked);
        });

    ui->projectionComboBox->addItem(tr("Equidistant"), QVariant::fromValue(
        Qn::FisheyeLensProjection::equidistant));
    ui->projectionComboBox->addItem(tr("Stereographic"), QVariant::fromValue(
        Qn::FisheyeLensProjection::stereographic));
    ui->projectionComboBox->addItem(tr("Equisolid"), QVariant::fromValue(
        Qn::FisheyeLensProjection::equisolid));

    connect(ui->projectionComboBox, QnComboboxCurrentIndexChanged, this,
        [this](int index)
        {
            ui->calibrateWidget->setProjection(ui->projectionComboBox->currentData()
                .value<Qn::FisheyeLensProjection>());

            emit dataChanged();
        });

    ui->mountComboBox->addItem(tr("Ceiling"), QVariant::fromValue(
        QnMediaDewarpingParams::VerticalDown));
    ui->mountComboBox->addItem(tr("Wall"), QVariant::fromValue(
        QnMediaDewarpingParams::Horizontal));
    ui->mountComboBox->addItem(tr("Floor/Table"), QVariant::fromValue(
        QnMediaDewarpingParams::VerticalUp));

    connect(ui->mountComboBox, QnComboboxCurrentIndexChanged,
        this, &FisheyeSettingsWidget::dataChanged);

    const QSize kSpacing = style::Metrics::kDefaultLayoutSpacing;

    anchorWidgetToParent(ui->autoCalibrationButton, Qt::RightEdge | Qt::BottomEdge,
        QMargins(0, 0, kSpacing.width(), kSpacing.height()));

    anchorWidgetToParent(ui->showGridCheckBox, Qt::LeftEdge | Qt::BottomEdge,
        QMargins(kSpacing.width(), 0, 0, kSpacing.height()));

    ui->calibrateWidget->setMinimumHeight(ui->autoCalibrationButton->sizeHint().height()
        + kSpacing.height() * 2);

    ui->autoCalibrationButton->hide();
    ui->showGridCheckBox->hide();

    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::previewImageChanged, this,
        [this]()
        {
            const bool hasImage = !ui->calibrateWidget->previewImage().isNull();
            ui->autoCalibrationButton->setVisible(hasImage);
            ui->showGridCheckBox->setVisible(hasImage);
        });
}

FisheyeSettingsWidget::~FisheyeSettingsWidget()
{
}

void FisheyeSettingsWidget::updateFromParams(const QnMediaDewarpingParams& params,
    ImageProvider* imageProvider)
{
    QSignalBlocker updateBlock(this);

    ui->mountComboBox->setCurrentIndex(ui->mountComboBox->findData(QVariant::fromValue(
        params.viewMode)));

    ui->showGridCheckBox->resize(ui->showGridCheckBox->minimumSizeHint());
    ui->autoCalibrationButton->resize(ui->autoCalibrationButton->minimumSizeHint());

    ui->fisheyeEnabledButton->setChecked(params.enabled);

    ui->angleSpinBox->setValue(params.fovRot);

    ui->showGridCheckBox->setChecked(qnSettings->isFisheyeCalibrationGridShown());

    if (ui->calibrateWidget->imageProvider() != imageProvider)
    {
        ui->calibrateWidget->init();
        ui->calibrateWidget->setImageProvider(imageProvider);
    }

    ui->projectionComboBox->setCurrentIndex(ui->projectionComboBox->findData(QVariant::fromValue(
        params.lensProjection)));

    ui->calibrateWidget->setRadius(params.radius);
    ui->calibrateWidget->setHorizontalStretch(params.hStretch);
    ui->calibrateWidget->setCenter(QPointF(params.xCenter, params.yCenter));

    ui->calibrateWidget->setEnabled(params.enabled);
    ui->settingsWidget->setEnabled(params.enabled);
}

QnMediaDewarpingParams FisheyeSettingsWidget::parameters() const
{
    QnMediaDewarpingParams result;
    result.enabled = ui->fisheyeEnabledButton->isChecked();
    result.fovRot = ui->angleSpinBox->value();
    result.viewMode = ui->mountComboBox->currentData().value<QnMediaDewarpingParams::ViewMode>();
    result.xCenter = ui->calibrateWidget->center().x();
    result.yCenter = ui->calibrateWidget->center().y();
    result.radius = ui->calibrateWidget->radius();
    result.hStretch = ui->calibrateWidget->horizontalStretch();
    result.lensProjection = ui->projectionComboBox->currentData()
        .value<Qn::FisheyeLensProjection>();

    return result;
}

bool FisheyeSettingsWidget::isReadOnly() const
{
    return ui->calibrateWidget->isReadOnly();
}

void FisheyeSettingsWidget::setReadOnly(bool value)
{
    ::setReadOnly(ui->fisheyeEnabledButton, value);
    ::setReadOnly(ui->mountComboBox, value);
    ::setReadOnly(ui->projectionComboBox, value);
    ::setReadOnly(ui->angleSpinBox, value);
    ::setReadOnly(ui->autoCalibrationButton, value);
    ::setReadOnly(ui->sizeSlider, value);
    ::setReadOnly(ui->xOffsetSlider, value);
    ::setReadOnly(ui->yOffsetSlider, value);
    ::setReadOnly(ui->ellipticitySlider, value);
    ui->calibrateWidget->setReadOnly(value);
}

} // namespace nx::vms::client::desktop
