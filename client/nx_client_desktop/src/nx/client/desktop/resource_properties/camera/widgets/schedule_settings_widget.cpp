#include "schedule_settings_widget.h"
#include "ui_schedule_settings_widget.h"

#include <QtGui/QStandardItemModel>

#include <QtWidgets/QListView>

#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/client/desktop/common/utils/stream_quality_strings.h>

#include <ui/common/read_only.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/event_processors.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"
#include "../utils/schedule_paint_functions.h"

namespace {

static constexpr int kRecordingTypeLabelFontSize = 12;
static constexpr int kRecordingTypeLabelFontWeight = QFont::DemiBold;
static constexpr int kCustomQualityOffset = 7;

template<class InputWidget>
float normalizedValue(const InputWidget* widget)
{
    return float(widget->value() - widget->minimum()) / (widget->maximum() - widget->minimum());
}

template<class InputWidget>
void setNormalizedValue(InputWidget* widget, float fraction)
{
    widget->setValue(widget->minimum() + fraction * (widget->maximum() - widget->minimum()));
}

} // namespace

namespace nx {
namespace client {
namespace desktop {

ScheduleSettingsWidget::ScheduleSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ScheduleSettingsWidget()),
    paintFunctions(new SchedulePaintFunctions())
{
    setupUi();
}

ScheduleSettingsWidget::~ScheduleSettingsWidget() = default;

void ScheduleSettingsWidget::setStore(CameraSettingsDialogStore* store)
{
    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &ScheduleSettingsWidget::loadState);

    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setScheduleBrushFps(value);
        });

    connect(ui->displayQualityCheckBox, &QCheckBox::stateChanged, store,
        [store](int state)
        {
            store->setRecordingShowQuality(state == Qt::Checked);
        });

    connect(ui->displayFpsCheckBox, &QCheckBox::stateChanged, store,
        [store](int state)
        {
            store->setRecordingShowFps(state == Qt::Checked);
        });

    const auto makeRecordingTypeHandler =
        [store](Qn::RecordingType value)
        {
            return
                [store, value](bool checked)
                {
                    if (checked)
                        store->setScheduleBrushRecordingType(value);
                };
        };

    connect(ui->recordAlwaysButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RecordingType::always));
    connect(ui->recordMotionButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RecordingType::motionOnly));
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RecordingType::motionAndLow));
    connect(ui->noRecordButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RecordingType::never));

    const auto qualityApproximation =
        [this](int index)
        {
            auto value = ui->qualityComboBox->itemData(index).toInt();
            if (value >= kCustomQualityOffset)
                value -= kCustomQualityOffset;
            return static_cast<Qn::StreamQuality>(value);
        };

    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, store,
        [store, qualityApproximation](int index)
        {
            store->setScheduleBrushQuality(qualityApproximation(index));
        });

    connect(ui->advancedSettingsButton, &QPushButton::clicked, store,
        [store]
        {
            store->toggleCustomBitrateVisible();
        });

    connect(ui->bitrateSpinBox, QnSpinboxDoubleValueChanged, store,
        [store](double value)
        {
            store->setCustomRecordingBitrateMbps(static_cast<float>(value));
        });

    connect(ui->bitrateSlider, &QSlider::valueChanged, store,
        [this, store](int /*value*/)
        {
            store->setCustomRecordingBitrateNormalized(
                normalizedValue(ui->bitrateSlider));
        });
}

void ScheduleSettingsWidget::setupUi()
{
    ui->setupUi(this);

    QFont labelFont;
    labelFont.setPixelSize(kRecordingTypeLabelFontSize);
    labelFont.setWeight(kRecordingTypeLabelFontWeight);

    for (auto label: {
        ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord
    })
    {
        label->setFont(labelFont);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
    }

    // Quality combo box contains Qn::StreamQuality constants as user data for normal visible items
    // (Low, Medium, High, Best), and those constants + customQualityOffset for invisible items
    // with asterisks (Low*, Medium*, High*, Best*).
    const auto addQualityItem =
        [this](Qn::StreamQuality quality)
        {
            const auto text = toDisplayString(quality);
            ui->qualityComboBox->addItem(text, (int)quality);
            const auto index = ui->qualityComboBox->count();
            ui->qualityComboBox->addItem(text + lit(" *"), kCustomQualityOffset + (int)quality);
            qobject_cast<QListView*>(ui->qualityComboBox->view())->setRowHidden(index, true);
            if (auto model = qobject_cast<QStandardItemModel*>(ui->qualityComboBox->model()))
                model->item(index)->setFlags(Qt::NoItemFlags);
        };

    addQualityItem(Qn::StreamQuality::low);
    addQualityItem(Qn::StreamQuality::normal);
    addQualityItem(Qn::StreamQuality::high);
    addQualityItem(Qn::StreamQuality::highest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData((int)Qn::StreamQuality::high));
    ui->qualityLabelHint->setHint(tr("Quality setting determines the compression rate only, and does not affect resolution. Low, Medium, High and Best are preset bitrate values."));

    ui->bitrateSpinBox->setSuffix(lit(" ") + tr("Mbit/s"));

    ui->bitrateSlider->setProperty(
        style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    auto aligner = new Aligner(this);
    aligner->addWidgets({ui->fpsLabel, ui->qualityLabel, ui->bitrateLabel});

    // Reset group box bottom margin to zero. Sub-widget margins defined in the ui-file rely on it.
    ui->settingsGroupBox->ensurePolished();
    auto margins = ui->settingsGroupBox->contentsMargins();
    margins.setBottom(0);
    ui->settingsGroupBox->setContentsMargins(margins);

    ui->recordAlwaysButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RecordingType::always));
    ui->recordMotionButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RecordingType::motionOnly));
    ui->recordMotionPlusLQButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RecordingType::motionAndLow));
    ui->noRecordButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RecordingType::never));

}

void ScheduleSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    if (!state.supportsSchedule())
        return;

    const auto& recording = state.recording;
    const auto& brush = recording.brush;
    const bool recordingParamsEnabled = brush.recordingType != Qn::RecordingType::never
        && recording.parametersAvailable;

    ui->recordAlwaysButton->setChecked(brush.recordingType == Qn::RecordingType::always);
    setReadOnly(ui->recordAlwaysButton, state.readOnly);

    const bool hasMotion = state.hasMotion();
    ui->recordMotionButton->setEnabled(hasMotion);
    ui->labelMotionOnly->setEnabled(hasMotion);
    if (hasMotion)
    {
        ui->recordMotionButton->setChecked(brush.recordingType == Qn::RecordingType::motionOnly);
        setReadOnly(ui->recordMotionButton, state.readOnly);
        ui->recordMotionButton->setToolTip({});
    }
    else
    {
        ui->recordMotionButton->setToolTip(motionOptionHint(state));
    }

    const bool hasDualStreaming = state.hasDualStreaming();
    ui->recordMotionPlusLQButton->setEnabled(hasDualStreaming);
    ui->labelMotionPlusLQ->setEnabled(hasDualStreaming);
    if (hasDualStreaming)
    {
        ui->recordMotionPlusLQButton->setChecked(brush.recordingType == Qn::RecordingType::motionAndLow);
        setReadOnly(ui->recordMotionPlusLQButton, state.readOnly);
        ui->recordMotionPlusLQButton->setToolTip({});
    }
    else
    {
        ui->recordMotionPlusLQButton->setToolTip(motionOptionHint(state));
    }

    ui->noRecordButton->setChecked(brush.recordingType == Qn::RecordingType::never);
    setReadOnly(ui->noRecordButton, state.readOnly);

    ui->fpsSpinBox->setEnabled(recordingParamsEnabled);
    ui->fpsSpinBox->setMaximum(state.maxRecordingBrushFps());
    ui->fpsSpinBox->setValue(brush.fps);
    setReadOnly(ui->fpsSpinBox, state.readOnly);

    const bool automaticBitrate = brush.isAutomaticBitrate();
    ui->qualityComboBox->setEnabled(recordingParamsEnabled);
    setReadOnly(ui->qualityComboBox, state.readOnly);
    ui->qualityComboBox->setCurrentIndex(
        ui->qualityComboBox->findData(
            (int)brush.quality
            + (automaticBitrate ? 0 : kCustomQualityOffset)));

    ui->advancedSettingsWidget->setVisible(
        recording.customBitrateAvailable
        && recording.customBitrateVisible);

    if (recording.customBitrateAvailable)
    {
        ui->advancedSettingsWidget->setEnabled(recordingParamsEnabled);
        ui->bitrateSpinBox->setRange(recording.minBitrateMbps, recording.maxBitrateMpbs);
        ui->bitrateSpinBox->setValue(recording.bitrateMbps);
        setReadOnly(ui->bitrateSpinBox, state.readOnly);
        setNormalizedValue(ui->bitrateSlider, recording.normalizedCustomBitrateMbps());
        setReadOnly(ui->bitrateSlider, state.readOnly);

        const auto buttonText = recording.customBitrateVisible
            ? tr("Less Settings")
            : tr("More Settings");
        const auto buttonIcon = qnSkin->icon(
            recording.customBitrateVisible
            ? lit("text_buttons/collapse.png")
            : lit("text_buttons/expand.png"));
        ui->advancedSettingsButton->setText(buttonText);
        ui->advancedSettingsButton->setIcon(buttonIcon);
    }

    ui->panicModeLabel->setText(state.panicMode ? tr("On") : tr("Off"));
    setWarningStyleOn(ui->panicModeLabel, state.panicMode);

    ui->settingsGroupBox->layout()->activate();

    const bool recordingEnabled = recording.enabled.valueOr(false);
    const auto labels =
        {ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord};
    for (auto label: labels)
    {
        const auto button = qobject_cast<QAbstractButton*>(label->buddy());
        const QPalette::ColorRole foreground = button && button->isChecked() && recordingEnabled
            ? QPalette::Highlight
            : QPalette::WindowText;
        label->setForegroundRole(foreground);
    }
}

QString ScheduleSettingsWidget::motionOptionHint(const CameraSettingsDialogState& state)
{
    using CombinedValue = CameraSettingsDialogState::CombinedValue;
    const bool devicesHaveMotion = state.devicesDescription.hasMotion == CombinedValue::All;
    const bool devicesHaveDS = state.devicesDescription.hasDualStreaming == CombinedValue::All;

    if (state.isSingleCamera())
    {
        if (devicesHaveMotion && !devicesHaveDS)
            return tr("Dual-Streaming not supported for this camera");

        if (!devicesHaveMotion && !devicesHaveDS)
            return tr("Dual-Streaming and motion detection not supported for this camera");

        const bool motionDetectionEnabled = state.singleCameraSettings.enableMotionDetection();
        if (!motionDetectionEnabled)
        {
            return tr("Motion detection disabled")
                + L'\n'
                + tr("To enable or adjust it, go to the \"Motion\" tab in Camera Settings");
        }

        return QString();
    }

    if (!devicesHaveMotion || !devicesHaveDS)
    {
        return tr("Motion detection disabled or not supported")
            + L'\n'
            + tr("To ensure it is supported and to enable it, "
                "go to the \"Motion\" tab in Camera Settings.");
    }

    return QString();
}


} // namespace desktop
} // namespace client
} // namespace nx
