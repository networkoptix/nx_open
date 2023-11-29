// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "schedule_settings_widget.h"
#include "ui_schedule_settings_widget.h"


#include <QtGui/QStandardItemModel>
#include <QtCore/QVariant>

#include <core/resource/device_dependent_strings.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/aligner.h>
#include <nx/vms/client/desktop/common/utils/combo_box_utils.h>
#include <nx/vms/client/desktop/common/utils/stream_quality_strings.h>
#include <nx/vms/client/desktop/resource_properties/schedule/record_schedule_cell_painter.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/helper.h>
#include <ui/common/read_only.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <utils/camera/camera_bitrate_calculator.h>
#include <utils/common/event_processors.h>

#include "../flux/camera_settings_dialog_state.h"
#include "../flux/camera_settings_dialog_store.h"

using RecordingType = nx::vms::api::RecordingType;
using RecordingMetadataTypes = nx::vms::api::RecordingMetadataTypes;
using StreamQuality = nx::vms::api::StreamQuality;

namespace {

static constexpr int kRecordingTypeLabelFontSize = 12;
static constexpr auto kRecordingTypeLabelFontWeight = QFont::Medium;
static constexpr int kCustomQualityOffset = 7;

static const QColor kLight10Color = "#A5B7C0";
static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight10Color, "light10"}, {kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight10Color, "light11"}, {kLight16Color, "light17"}}},
    {QIcon::Selected, {{kLight16Color, "light15"}}},
};

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

QVariant buttonBrush(
    RecordingType recordingType,
    RecordingMetadataTypes metadataTypes)
{
    nx::vms::client::desktop::RecordScheduleCellData cellData;

    cellData.recordingType = recordingType;
    cellData.metadataTypes = metadataTypes;

    return QVariant::fromValue<nx::vms::client::desktop::RecordScheduleCellData>(cellData);
}

nx::vms::client::desktop::ScheduleBrushSelectableButton* buttonForRecordingType(
    Ui::ScheduleSettingsWidget* ui,
    RecordingType recordingType)
{
    switch (recordingType)
    {
        case RecordingType::always:
            return ui->recordAlwaysButton;
        case RecordingType::metadataOnly:
            return ui->recordMetadataButton;
        case RecordingType::never:
            return ui->noRecordButton;
        case RecordingType::metadataAndLowQuality:
            return ui->recordMetadataPlusLQButton;
        default:
            NX_ASSERT(false, "Unexpected recording type");
            break;
    }
    return nullptr;
}

} // namespace

namespace nx::vms::client::desktop {

using MetadataRadioButton = CameraSettingsDialogState::MetadataRadioButton;

ScheduleSettingsWidget::ScheduleSettingsWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::ScheduleSettingsWidget())
{
    setupUi();
}

ScheduleSettingsWidget::~ScheduleSettingsWidget()
{
    // Required here for forward-declared scoped pointer destruction.
}

void ScheduleSettingsWidget::setStore(CameraSettingsDialogStore* store)
{
    connect(store, &CameraSettingsDialogStore::stateChanged, this,
        &ScheduleSettingsWidget::loadState);

    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged, store,
        [store](int value)
        {
            store->setScheduleBrushFps(value);
        });

    connect(ui->displayQualityButton, &QPushButton::clicked, store,
        [store]() { store->setRecordingShowQuality(!store->state().recording.showQuality); });

    connect(ui->displayFpsButton, &QPushButton::clicked, store,
        [store]() { store->setRecordingShowFps(!store->state().recording.showFps); });

    const auto makeRecordingTypeHandler =
        [store](RecordingType recordingType)
        {
            return
                [store, recordingType](bool checked)
                {
                    if (checked)
                        store->setScheduleBrushRecordingType(recordingType);
                };
        };

    const auto makeRecordingMetadataRadioButtonHandler =
        [store](MetadataRadioButton value)
        {
            return
                [store, value](bool checked)
                {
                    if (checked)
                        store->setRecordingMetadataRadioButton(value);
                };
        };

    connect(ui->recordAlwaysButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(RecordingType::always));
    connect(ui->recordMetadataButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(RecordingType::metadataOnly));
    connect(ui->recordMetadataPlusLQButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(RecordingType::metadataAndLowQuality));
    connect(ui->noRecordButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(RecordingType::never));

    connect(ui->motionMetadataRadioButton, &QRadioButton::toggled, store,
        makeRecordingMetadataRadioButtonHandler(MetadataRadioButton::motion));
    connect(ui->objectsMetadataRadioButton, &QRadioButton::toggled, store,
        makeRecordingMetadataRadioButtonHandler(MetadataRadioButton::objects));
    connect(ui->motionAndObjectsMetadataRadioButton, &QRadioButton::toggled, store,
        makeRecordingMetadataRadioButtonHandler(MetadataRadioButton::both));

    const auto qualityApproximation =
        [this](int index)
        {
            auto value = ui->qualityComboBox->itemData(index).toInt();
            if (value >= kCustomQualityOffset)
                value -= kCustomQualityOffset;
            return static_cast<StreamQuality>(value);
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
            store->setRecordingBitrateMbps(static_cast<float>(value));
        });

    connect(ui->bitrateSlider, &QSlider::valueChanged, store,
        [this, store](int)
        {
            store->setRecordingBitrateNormalized(normalizedValue(ui->bitrateSlider));
        });
}


void ScheduleSettingsWidget::setScheduleCellPainter(ScheduleCellPainter* cellPainter)
{
    ui->recordAlwaysButton->setCellPainter(cellPainter);
    ui->recordMetadataButton->setCellPainter(cellPainter);
    ui->recordMetadataPlusLQButton->setCellPainter(cellPainter);
    ui->noRecordButton->setCellPainter(cellPainter);
}

void ScheduleSettingsWidget::setupUi()
{
    ui->setupUi(this);

    QFont labelFont;
    labelFont.setPixelSize(kRecordingTypeLabelFontSize);
    labelFont.setWeight(kRecordingTypeLabelFontWeight);

    const auto labels =
        {ui->labelAlways, ui->labelMetadataOnly, ui->labelMetadataPlusLQ, ui->labelNoRecord};

    for (auto label: labels)
    {
        label->setFont(labelFont);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
    }

    // Quality combo box contains StreamQuality constants as user data for normal visible items
    // (Low, Medium, High, Best), and those constants + customQualityOffset for invisible items
    // with asterisks (Low*, Medium*, High*, Best*).
    const auto addQualityItem =
        [this](StreamQuality streamQuality)
        {
            const auto text = toDisplayString(streamQuality);

            ui->qualityComboBox->addItem(text, static_cast<int>(streamQuality));
            combo_box_utils::addHiddenItem(ui->qualityComboBox,
                text + " *", kCustomQualityOffset + static_cast<int>(streamQuality));
        };

    addQualityItem(StreamQuality::low);
    addQualityItem(StreamQuality::normal);
    addQualityItem(StreamQuality::high);
    addQualityItem(StreamQuality::highest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData((int)StreamQuality::high));
    ui->qualityLabel->setHint(tr("Quality setting determines the compression rate only, "
        "and does not affect resolution. Low, Medium, High and Best are preset bitrate values."));

    ui->bitrateSpinBox->setDecimals(common::CameraBitrateCalculator::kBitrateKbpsPrecisionDecimals);
    ui->bitrateSpinBox->setSuffix(QString(" ") + tr("Mbit/s"));

    ui->bitrateSlider->setProperty(
        style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    auto aligner = new Aligner(this);
    aligner->addWidgets({ui->fpsLabel, ui->qualityLabel, ui->bitrateLabel});

    ui->recordAlwaysButton->setButtonBrush(buttonBrush(RecordingType::always, {}));
    ui->noRecordButton->setButtonBrush(buttonBrush(RecordingType::never, {}));

    auto buttonAligner = new Aligner(this);
    buttonAligner->addWidgets(
        {ui->labelAlways, ui->labelMetadataOnly, ui->labelMetadataPlusLQ, ui->labelNoRecord});
}

void ScheduleSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    if (!state.supportsSchedule())
        return;

    const bool cameraControlEnabled = state.cameraControlEnabled();

    ui->displayQualityButton->setEnabled(cameraControlEnabled);
    ui->displayFpsButton->setEnabled(cameraControlEnabled);

    const auto& recording = state.recording;
    const auto& brush = recording.brush;
    const bool recordingParamsEnabled = brush.recordingType != RecordingType::never
        && recording.parametersAvailable && cameraControlEnabled;

    const QString cameraControlTooltip =
        [&state]()
        {
            if (!state.settingsOptimizationEnabled)
            {
                return tr("Quality and FPS settings are disabled because of the "
                    "\"Allow system to optimize camera settings\" option in the "
                    "\"System Administration\" dialog.\n"
                    "You can still change quality and FPS directly on the camera.");
            }

            if (!state.expert.cameraControlDisabled.hasValue())
                return tr("Quality and FPS settings are disabled for some of the selected cameras.");

            if (state.expert.cameraControlDisabled())
                return tr("Quality and FPS settings are disabled because of the "
                    "\"Keep camera stream and profile settings\" option on the \"Expert\" tab.\n"
                    "You can still change quality and FPS directly on the camera.");

            return QString();
        }();

    ui->displayQualityButton->setToolTip(cameraControlTooltip);
    ui->displayFpsButton->setToolTip(cameraControlTooltip);
    ui->qualityComboBox->setToolTip(cameraControlTooltip);
    ui->fpsSpinBox->setToolTip(cameraControlTooltip);

    setupRecordingAndMetadataTypeControls(state);

    ui->fpsSpinBox->setEnabled(recordingParamsEnabled);
    ui->fpsSpinBox->setMaximum(state.maxRecordingBrushFps());
    ui->fpsSpinBox->setValue(brush.fps);
    setReadOnly(ui->fpsSpinBox, state.readOnly);

    ui->qualityComboBox->setEnabled(recordingParamsEnabled);
    setReadOnly(ui->qualityComboBox, state.readOnly);

    const auto qualityItemModel = qobject_cast<QStandardItemModel*>(ui->qualityComboBox->model());
    NX_CRITICAL(qualityItemModel);

    const auto setItemEnabled =
        [model = qualityItemModel](int index, bool value)
        {
            const auto item = model->item(index);
            if (!NX_ASSERT(item))
                return;

            auto flags = item->flags();
            flags.setFlag(Qt::ItemIsEnabled, value);
            flags.setFlag(Qt::ItemIsSelectable, value);
            item->setFlags(flags);
        };

    for (int i = 0; i < ui->qualityComboBox->count(); ++i)
    {
        int quality = ui->qualityComboBox->itemData(i).toInt();
        if (quality >= kCustomQualityOffset)
            quality -= kCustomQualityOffset;

        setItemEnabled(i, quality >= static_cast<int>(state.recording.minRelevantQuality));
    }

    const bool fpsLockedBitrate =
        qFuzzyEquals(state.recording.maxBitrateMpbs, state.recording.minBitrateMbps);

    if (fpsLockedBitrate)
    {
        ui->qualityComboBox->setCurrentIndex(
            ui->qualityComboBox->findData(static_cast<int>(StreamQuality::highest)));
    }
    else if (brush.streamQuality < state.recording.minRelevantQuality)
    {
        ui->qualityComboBox->setCurrentIndex(
            ui->qualityComboBox->findData(static_cast<int>(state.recording.minRelevantQuality)));
    }
    else
    {
        ui->qualityComboBox->setCurrentIndex(
            ui->qualityComboBox->findData(static_cast<int>(brush.streamQuality)
                + (brush.isAutoBitrate() ? 0 : kCustomQualityOffset)));
    }

    const bool customBitrateAvailable = recording.customBitrateAvailable && cameraControlEnabled;

    ui->advancedSettingsButton->setVisible(customBitrateAvailable);
    ui->advancedSettingsWidget->setVisible(customBitrateAvailable
        && recording.customBitrateVisible
        && recording.parametersAvailable);

    if (customBitrateAvailable)
    {
        ui->advancedSettingsWidget->setEnabled(recordingParamsEnabled && !fpsLockedBitrate);
        ui->bitrateSpinBox->setRange(recording.minBitrateMbps, recording.maxBitrateMpbs);
        ui->bitrateSpinBox->setValue(recording.bitrateMbps);
        setReadOnly(ui->bitrateSpinBox, state.readOnly);
        setNormalizedValue(ui->bitrateSlider,
            fpsLockedBitrate ? 1.0 : recording.normalizedCustomBitrateMbps());
        setReadOnly(ui->bitrateSlider, state.readOnly);

        const auto buttonText = recording.customBitrateVisible
            ? tr("Less Settings")
            : tr("More Settings");

        const auto buttonIcon = recording.customBitrateVisible
            ? qnSkin->icon("text_buttons/arrow_up_20.svg", kIconSubstitutions)
            : qnSkin->icon("text_buttons/arrow_down_20.svg", kIconSubstitutions);

        ui->advancedSettingsButton->setText(buttonText);
        ui->advancedSettingsButton->setIcon(buttonIcon);
    }

    const auto eyeIcon = qnSkin->icon("text_buttons/eye_open_20.svg", kIconSubstitutions);
    const auto closedEyeIcon = qnSkin->icon("text_buttons/eye_closed_20.svg", kIconSubstitutions);

    ui->displayQualityButton->setIcon(recording.showQuality && cameraControlEnabled
        ? eyeIcon
        : closedEyeIcon);

    ui->displayFpsButton->setIcon(recording.showFps && cameraControlEnabled
        ? eyeIcon
        : closedEyeIcon);

    ui->fpsAndQualityWidget->setVisible(recording.parametersAvailable);

    ui->settingsGroupBox->layout()->activate();
    layout()->activate();

    const bool recordingEnabled = recording.enabled.valueOr(false);
    const auto labels =
        {ui->labelAlways, ui->labelMetadataOnly, ui->labelMetadataPlusLQ, ui->labelNoRecord};

    for (auto label: labels)
    {
        const auto button = qobject_cast<QAbstractButton*>(label->buddy());
        const QPalette::ColorRole foreground = button && button->isChecked() && recordingEnabled
            ? QPalette::Highlight
            : QPalette::WindowText;
        label->setForegroundRole(foreground);
    }
}

void ScheduleSettingsWidget::setupRecordingAndMetadataTypeControls(
    const CameraSettingsDialogState& state)
{
    // Controls enabled state.
    const bool hasRecording = state.recording.enabled.valueOr(false);
    const bool hasMotion = state.isMotionDetectionActive();
    const bool hasObjectDetection = state.isObjectDetectionSupported();
    const bool hasDualStreaming = state.isDualStreamingEnabled();
    const bool hasMetadata = hasMotion || hasObjectDetection;
    const auto appliedMetadataTypes = CameraSettingsDialogState::radioButtonToMetadataTypes(
        state.recording.metadataRadioButton);

    // Setup "Record by metadata" button.
    {
        ui->labelMetadataOnly->setEnabled(hasMetadata);

        auto button = ui->recordMetadataButton;
        button->setEnabled(hasMetadata);
        button->setButtonBrush(buttonBrush(RecordingType::metadataOnly, appliedMetadataTypes));

        QString recordMetadataButtonTooltip;
        if (hasRecording && !hasMetadata)
            recordMetadataButtonTooltip = disabledMotionDetectionTooltip(state);

        button->setToolTip(recordMetadataButtonTooltip);
    }

    // Setup "Record by metadata + LQ" button.
    {
        ui->labelMetadataPlusLQ->setEnabled(hasMetadata && hasDualStreaming);

        auto button = ui->recordMetadataPlusLQButton;
        button->setEnabled(hasMetadata && hasDualStreaming);
        button->setButtonBrush(
            buttonBrush(RecordingType::metadataAndLowQuality, appliedMetadataTypes));

        QString metadataPlusLQTooltip;
        if (hasRecording && !hasMetadata)
            metadataPlusLQTooltip = disabledMotionDetectionTooltip(state);
        else if (hasRecording && !hasDualStreaming)
            metadataPlusLQTooltip = disabledMotionPlusLqTooltip(state);

        button->setToolTip(metadataPlusLQTooltip);
    }

    // Setup metadata type radiobuttons.
    ui->motionMetadataRadioButton->setEnabled(hasMotion);
    ui->objectsMetadataRadioButton->setEnabled(hasObjectDetection);
    ui->motionAndObjectsMetadataRadioButton->setEnabled(hasMotion && hasObjectDetection);

    // Brush buttons and radiobuttons.
    switch (state.recording.metadataRadioButton)
    {
        case MetadataRadioButton::objects:
            ui->objectsMetadataRadioButton->setChecked(true);
            ui->labelMetadataOnly->setText(tr("Objects Only"));
            ui->labelMetadataPlusLQ->setText(tr("Objects\n + Low-Res"));
            break;

        case MetadataRadioButton::both:
            ui->motionAndObjectsMetadataRadioButton->setChecked(true);
            ui->labelMetadataOnly->setText(tr("Motion, Objects"));
            ui->labelMetadataPlusLQ->setText(tr("Motion, Objects\n + Low-Res"));
            break;

        case MetadataRadioButton::motion:
        default:
            ui->motionMetadataRadioButton->setChecked(true);
            ui->labelMetadataOnly->setText(tr("Motion Only"));
            ui->labelMetadataPlusLQ->setText(tr("Motion\n + Low-Res"));
            break;
    }

    QString motionDetectionTooltip;
    if (!hasMotion && hasRecording)
        motionDetectionTooltip = disabledMotionDetectionTooltip(state);
    ui->motionMetadataRadioButton->setToolTip(motionDetectionTooltip);

    QString objectDetectionTooltip;
    if (!hasObjectDetection && hasRecording)
        objectDetectionTooltip = disabledObjectDetectionTooltip(state);
    ui->objectsMetadataRadioButton->setToolTip(objectDetectionTooltip);

    QString motionAndObjectDetectionTooltip;
    if (!(hasMotion && hasObjectDetection) && hasRecording)
    {
        motionAndObjectDetectionTooltip = hasMotion
            ? disabledObjectDetectionTooltip(state)
            : disabledMotionAndObjectDetectionTooltip(state);
    }
    ui->motionAndObjectsMetadataRadioButton->setToolTip(motionAndObjectDetectionTooltip);

    const auto recordingType = state.recording.brush.recordingType;
    buttonForRecordingType(ui.get(), recordingType)->setChecked(true);

    // Controls read only state.
    setReadOnly(ui->recordAlwaysButton, state.readOnly);
    setReadOnly(ui->recordMetadataButton, state.readOnly);
    setReadOnly(ui->recordMetadataPlusLQButton, state.readOnly);
    setReadOnly(ui->noRecordButton, state.readOnly);
}

QString ScheduleSettingsWidget::motionOptionHint(const CameraSettingsDialogState& state)
{
    if (!state.isMotionDetectionActive())
        return tr("Motion detection is disabled or not supported");

    if (!state.isDualStreamingEnabled())
        return tr("Dual-streaming is disabled or not supported");

    return QString();
}

QString ScheduleSettingsWidget::disabledMotionDetectionTooltip(
    const CameraSettingsDialogState& state)
{
    return QnCameraDeviceStringSet{
        "<unused>",
        tr("Motion detection is disabled for some of the selected devices"),
        tr("Motion detection is disabled for this camera"),
        tr("Motion detection is disabled for some of the selected cameras"),
        tr("Motion detection is disabled for this I/O module"),
        tr("Motion detection is disabled for some of the selected I/O modules")
    }.getString(state.deviceType, state.devicesCount > 1);
}

QString ScheduleSettingsWidget::disabledMotionPlusLqTooltip(
    const CameraSettingsDialogState& state)
{
    return QnCameraDeviceStringSet{
        "<unused>",
        tr("Some of the selected devices have only one stream. Recordings with and without "
            "motion will share the same resolution or quality."),
        tr("This camera has only one stream. Recordings with and without motion will share "
            "the same resolution or quality."),
        tr("Some of the selected cameras have only one stream. Recordings with and without "
            "motion will share the same resolution or quality."),
        tr("This I/O module has only one stream. Recordings with and without motion will "
            "share the same resolution or quality."),
        tr("Some of the selected I/O modules have only one stream. Recordings with and "
            "without motion will share the same resolution or quality.")
    }.getString(state.deviceType, state.devicesCount > 1);
}

QString ScheduleSettingsWidget::disabledObjectDetectionTooltip(
    const CameraSettingsDialogState& state)
{
    return QnCameraDeviceStringSet{
        "<unused>",
        tr("Object detection is disabled for some of the selected devices"),
        tr("Object detection is disabled for this camera"),
        tr("Object detection is disabled for some of the selected cameras"),
        tr("Object detection is disabled for this I/O module"),
        tr("Object detection is disabled for some of the selected I/O modules")
    }.getString(state.deviceType, state.devicesCount > 1);
}

QString ScheduleSettingsWidget::disabledMotionAndObjectDetectionTooltip(
    const CameraSettingsDialogState& state)
{
    return QnCameraDeviceStringSet{
        "<unused>",
        tr("Motion & object detection is disabled for some of the selected devices"),
        tr("Motion & object detection is disabled for this camera"),
        tr("Motion & object detection is disabled for some of the selected cameras"),
        tr("Motion & object detection is disabled for this I/O module"),
        tr("Motion & object detection is disabled for some of the selected I/O modules")
    }.getString(state.deviceType, state.devicesCount > 1);
}

} // namespace nx::vms::client::desktop
