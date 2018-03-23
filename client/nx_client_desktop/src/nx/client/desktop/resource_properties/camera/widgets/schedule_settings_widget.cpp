#include "schedule_settings_widget.h"
#include "ui_schedule_settings_widget.h"

#include <QtGui/QStandardItemModel>
#include <QtWidgets/QListView>

#include <core/resource/media_resource.h>

#include <ui/common/aligner.h>
#include <ui/style/helper.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>

#include <utils/common/event_processors.h>

#include "../redux/camera_settings_dialog_state.h"
#include "../redux/camera_settings_dialog_store.h"

namespace {

static constexpr int kRecordingTypeLabelFontSize = 12;
static constexpr int kRecordingTypeLabelFontWeight = QFont::DemiBold;
static constexpr int kCustomQualityOffset = Qn::StreamQualityCount;

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
    ui(new Ui::ScheduleSettingsWidget())
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
            ui->qualityComboBox->addItem(text, quality);
            const auto index = ui->qualityComboBox->count();
            ui->qualityComboBox->addItem(text + lit(" *"), kCustomQualityOffset + quality);
            qobject_cast<QListView*>(ui->qualityComboBox->view())->setRowHidden(index, true);
            if (auto model = qobject_cast<QStandardItemModel*>(ui->qualityComboBox->model()))
                model->item(index)->setFlags(Qt::NoItemFlags);
        };

    addQualityItem(Qn::QualityLow);
    addQualityItem(Qn::QualityNormal);
    addQualityItem(Qn::QualityHigh);
    addQualityItem(Qn::QualityHighest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(Qn::QualityHigh));

    ui->bitrateSpinBox->setSuffix(lit(" ") + tr("Mbit/s"));

    ui->bitrateSlider->setProperty(
        style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    /*
    installEventHandler(
        {ui->recordMotionButton, ui->recordMotionPlusLQButton},
        QEvent::MouseButtonRelease,
        this,
        &ScheduleSettingsWidget::at_releaseSignalizer_activated);
    */

    auto aligner = new QnAligner(this);
    aligner->addWidgets({ui->fpsLabel, ui->qualityLabel, ui->bitrateLabel});

    // Reset group box bottom margin to zero. Sub-widget margins defined in the ui-file rely on it.
    ui->settingsGroupBox->ensurePolished();
    auto margins = ui->settingsGroupBox->contentsMargins();
    margins.setBottom(0);
    ui->settingsGroupBox->setContentsMargins(margins);
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
        makeRecordingTypeHandler(Qn::RT_Always));
    connect(ui->recordMotionButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RT_MotionOnly));
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RT_MotionAndLowQuality));
    connect(ui->noRecordButton, &QToolButton::toggled, store,
        makeRecordingTypeHandler(Qn::RT_Never));

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

/*
void ScheduleSettingsWidget::afterContextInitialized()
{
    connect(
        context()->instance<QnWorkbenchPanicWatcher>(),
        &QnWorkbenchPanicWatcher::panicModeChanged,
        this,
        &ScheduleSettingsWidget::updatePanicLabelText);
    updatePanicLabelText();
}
*/

void ScheduleSettingsWidget::loadState(const CameraSettingsDialogState& state)
{
    const auto& recording = state.recording;
    const auto& brush = recording.brush;

    const bool recordingParamsEnabled = brush.recordingType != Qn::RT_Never
        && recording.parametersAvailable;

    ui->recordAlwaysButton->setChecked(brush.recordingType == Qn::RT_Always);
    ui->recordMotionButton->setChecked(brush.recordingType == Qn::RT_MotionOnly);
    ui->recordMotionPlusLQButton->setChecked(brush.recordingType == Qn::RT_MotionAndLowQuality);
    ui->noRecordButton->setChecked(brush.recordingType == Qn::RT_Never);

    ui->fpsSpinBox->setEnabled(recordingParamsEnabled);
    ui->fpsSpinBox->setMaximum(recording.maxBrushFps());
    ui->fpsSpinBox->setValue(brush.fps);

    const bool automaticBitrate = brush.isAutomaticBitrate()
        /* && recording.customBitrateAvailable */;
    ui->qualityComboBox->setEnabled(recordingParamsEnabled);
    ui->qualityComboBox->setCurrentIndex(
        ui->qualityComboBox->findData(
            brush.quality
            + (automaticBitrate ? 0 : kCustomQualityOffset)));

    ui->advancedSettingsWidget->setVisible(
        recording.customBitrateAvailable
        && recording.customBitrateVisible);

    if (recording.customBitrateAvailable)
    {
        ui->advancedSettingsWidget->setEnabled(recordingParamsEnabled);
        ui->bitrateSpinBox->setRange(recording.minBitrateMbps, recording.maxBitrateMpbs);
        ui->bitrateSpinBox->setValue(recording.bitrateMbps);
        setNormalizedValue(ui->bitrateSlider, recording.normalizedCustomBitrateMbps());

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

    ui->settingsGroupBox->layout()->activate();
}

/*
void ScheduleSettingsWidget::setReadOnly(bool readOnly)
{
    using ::setReadOnly;
    setReadOnly(ui->recordAlwaysButton, readOnly);
    // TODO: #GDM This is not valid. Camera may not support HW motion, we need to check for this.
    setReadOnly(ui->recordMotionButton, readOnly);
    setReadOnly(ui->recordMotionPlusLQButton, readOnly);
    setReadOnly(ui->noRecordButton, readOnly);
    setReadOnly(ui->qualityComboBox, readOnly);
    setReadOnly(ui->bitrateSlider, readOnly);
    setReadOnly(ui->bitrateSpinBox, readOnly);
    setReadOnly(ui->fpsSpinBox, readOnly);
}


void ScheduleSettingsWidget::updateScheduleTypeControls()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
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

void ScheduleSettingsWidget::updatePanicLabelText()
{
    ui->panicModeLabel->setText(tr("Off"));
    ui->panicModeLabel->setPalette(this->palette());

    if (!context())
        return;

    if (context()->instance<QnWorkbenchPanicWatcher>()->isPanicMode())
    {
        QPalette palette = this->palette();
        palette.setColor(QPalette::WindowText, QColor(255, 0, 0));
        ui->panicModeLabel->setPalette(palette);
        ui->panicModeLabel->setText(tr("On"));
    }
}

void ScheduleSettingsWidget::at_releaseSignalizer_activated(QObject *target)
{
    QWidget *widget = qobject_cast<QWidget *>(target);
    if (!widget)
        return;

    if (m_cameras.isEmpty() || widget->isEnabled() || (widget->parentWidget() && !widget->parentWidget()->isEnabled()))
        return;

    using boost::algorithm::all_of;

    if (m_cameras.size() > 1)
    {
        QnMessageBox::warning(this,
            tr("Motion detection disabled or not supported"),
            tr("To ensure it is supported and to enable it, go to the \"Motion\" tab in Camera Settings."));
    }
    else // One camera.
    {
        NX_ASSERT(m_cameras.size() == 1, Q_FUNC_INFO, "Following options are valid only for singular camera");
        QnVirtualCameraResourcePtr camera = m_cameras.first();

        // TODO: #GDM #Common duplicate code.
        bool hasDualStreaming = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasDualStreaming2(); });
        bool hasMotion = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasMotion(); });

        if (hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming not supported for this camera"));
        }
        else if (!hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming and motion detection not supported for this camera"));
        }
        else /// Has dual streaming but not motion.
        {
            QnMessageBox::warning(this,
                tr("Motion detection disabled"),
                tr("To enable or adjust it, go to the \"Motion\" tab in Camera Settings."));
        }
    }
}
*/

} // namespace desktop
} // namespace client
} // namespace nx
