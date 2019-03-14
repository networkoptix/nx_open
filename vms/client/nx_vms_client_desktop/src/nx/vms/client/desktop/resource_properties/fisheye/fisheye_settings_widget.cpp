#include "fisheye_settings_widget.h"
#include "ui_fisheye_settings_widget.h"
#include "fisheye_calibration_widget.h"

#include <QtCore/QSignalBlocker>

#include <ui/common/read_only.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/common/event_processors.h>

namespace nx::vms::client::desktop {

FisheyeSettingsWidget::FisheyeSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::FisheyeSettingsWidget)
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::CameraSettings_Dewarping_Help);

    ui->angleCorrectionHint->setHint(tr("Use this setting to compensate for distortion if camera is not mounted exactly vertically or horizontally."));
    setHelpTopic(ui->angleCorrectionHint, Qn::CameraSettings_Dewarping_Help);

    ui->sizeIcon1->setPixmap(qnSkin->pixmap("fisheye/circle_small.png"));
    ui->sizeIcon2->setPixmap(qnSkin->pixmap("fisheye/circle_big.png"));
    ui->xOffsetIcon1->setPixmap(qnSkin->pixmap("fisheye/arrow_left.png"));
    ui->xOffsetIcon2->setPixmap(qnSkin->pixmap("fisheye/arrow_right.png"));
    ui->yOffsetIcon1->setPixmap(qnSkin->pixmap("fisheye/arrow_down.png"));
    ui->yOffsetIcon2->setPixmap(qnSkin->pixmap("fisheye/arrow_up.png"));
    ui->ellipticityIcon1->setPixmap(qnSkin->pixmap("fisheye/ellipse_vertical.png"));
    ui->ellipticityIcon2->setPixmap(qnSkin->pixmap("fisheye/ellipse_horizontal.png"));

    connect(ui->angleSpinBox, QnDoubleSpinBoxValueChanged, this, &FisheyeSettingsWidget::dataChanged);
    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::dataChanged, this, &FisheyeSettingsWidget::dataChanged);

    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, ui->calibrateWidget,
        &FisheyeCalibrationWidget::setEnabled);
    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, ui->settingsWidget,
        &FisheyeSettingsWidget::setEnabled);
    connect(ui->fisheyeEnabledButton, &QPushButton::toggled, this,
        &FisheyeSettingsWidget::dataChanged);

    connect(ui->horizontalRadioButton, &QPushButton::clicked, this, &FisheyeSettingsWidget::dataChanged);
    connect(ui->viewDownButton,        &QPushButton::clicked, this, &FisheyeSettingsWidget::dataChanged);
    connect(ui->viewUpButton,          &QPushButton::clicked, this, &FisheyeSettingsWidget::dataChanged);

    connect(ui->sizeSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->sizeSlider);
            ui->calibrateWidget->setRadius(value * 0.01);
        });

    connect(ui->xOffsetSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->xOffsetSlider);
            ui->calibrateWidget->setCenter(QPointF(value * 0.01, ui->calibrateWidget->center().y()));
        });

    connect(ui->yOffsetSlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->yOffsetSlider);
            ui->calibrateWidget->setCenter(QPointF(ui->calibrateWidget->center().x(), 1.0 - value * 0.01));
        });

    connect(ui->ellipticitySlider, &QSlider::valueChanged, this,
        [this](int value)
        {
            QSignalBlocker signalBlocker(ui->ellipticitySlider);
            ui->calibrateWidget->setHorizontalStretch(value / 50.0);
        });

    connect(ui->calibrateWidget, &FisheyeCalibrationWidget::dataChanged, this,
        [this]()
        {
            ui->sizeSlider->setValue(static_cast<int>(ui->calibrateWidget->radius() * 100.0));
            ui->ellipticitySlider->setValue(static_cast<int>(ui->calibrateWidget->horizontalStretch() * 50.0));
            ui->xOffsetSlider->setValue(static_cast<int>(ui->calibrateWidget->center().x() * 100.0));
            ui->yOffsetSlider->setValue(static_cast<int>((1.0 - ui->calibrateWidget->center().y()) * 100.0));
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

    const QSize kSpacing = style::Metrics::kDefaultLayoutSpacing;
    const auto updateAutoCalibrationButtonGeometry =
        [this, kSpacing]()
        {
            const auto size = ui->autoCalibrationButton->sizeHint();
            auto parentSize = ui->calibrateWidget->geometry().size();
            const int x = parentSize.width() - size.width() - kSpacing.width();
            const int y = parentSize.height() - size.height() - kSpacing.height();

            ui->autoCalibrationButton->setGeometry(QRect(QPoint(x, y), size));
        };

    installEventHandler(ui->calibrateWidget, QEvent::Resize, this,
        updateAutoCalibrationButtonGeometry);

    updateAutoCalibrationButtonGeometry();

    ui->calibrateWidget->setMinimumHeight(ui->autoCalibrationButton->sizeHint().height()
        + kSpacing.height() * 2);
}

FisheyeSettingsWidget::~FisheyeSettingsWidget()
{
}

void FisheyeSettingsWidget::updateFromParams(const QnMediaDewarpingParams& params, ImageProvider* imageProvider)
{
    QSignalBlocker updateBlock(this);

    switch (params.viewMode)
    {
        case QnMediaDewarpingParams::Horizontal:
            ui->horizontalRadioButton->setChecked(true);
            break;
        case QnMediaDewarpingParams::VerticalUp:
            ui->viewUpButton->setChecked(true);
            break;
        case QnMediaDewarpingParams::VerticalDown:
            ui->viewDownButton->setChecked(true);
            break;
        default:
            NX_ASSERT(false, "Unsupported value");
    }

    ui->fisheyeEnabledButton->setChecked(params.enabled);

    ui->angleSpinBox->setValue(params.fovRot);

    if (ui->calibrateWidget->imageProvider() != imageProvider)
    {
        ui->calibrateWidget->init();
        ui->calibrateWidget->setImageProvider(imageProvider);
    }

    ui->calibrateWidget->setRadius(params.radius);
    ui->calibrateWidget->setHorizontalStretch(params.hStretch);
    ui->calibrateWidget->setCenter(QPointF(params.xCenter, params.yCenter));

    ui->calibrateWidget->setEnabled(params.enabled);
    ui->settingsWidget->setEnabled(params.enabled);
}

void FisheyeSettingsWidget::submitToParams(QnMediaDewarpingParams& params)
{
    params.enabled = ui->fisheyeEnabledButton->isChecked();
    params.fovRot = ui->angleSpinBox->value();

    if (ui->horizontalRadioButton->isChecked())
        params.viewMode = QnMediaDewarpingParams::Horizontal;
    else if (ui->viewDownButton->isChecked())
        params.viewMode = QnMediaDewarpingParams::VerticalDown;
    else
        params.viewMode = QnMediaDewarpingParams::VerticalUp;

    params.xCenter = ui->calibrateWidget->center().x();
    params.yCenter = ui->calibrateWidget->center().y();
    params.radius = ui->calibrateWidget->radius();
    params.hStretch = ui->calibrateWidget->horizontalStretch();
}

bool FisheyeSettingsWidget::isReadOnly() const
{
    return ui->calibrateWidget->isReadOnly();
}

void FisheyeSettingsWidget::setReadOnly(bool value)
{
    ::setReadOnly(ui->fisheyeEnabledButton, value);
    ::setReadOnly(ui->horizontalRadioButton, value);
    ::setReadOnly(ui->viewUpButton, value);
    ::setReadOnly(ui->viewDownButton, value);
    ::setReadOnly(ui->angleSpinBox, value);
    ::setReadOnly(ui->autoCalibrationButton, value);
    ::setReadOnly(ui->sizeSlider, value);
    ::setReadOnly(ui->xOffsetSlider, value);
    ::setReadOnly(ui->yOffsetSlider, value);
    ::setReadOnly(ui->ellipticitySlider, value);
    ui->calibrateWidget->setReadOnly(value);
}

} // namespace nx::vms::client::desktop
