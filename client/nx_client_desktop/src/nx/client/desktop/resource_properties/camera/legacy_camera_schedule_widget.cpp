#include "legacy_camera_schedule_widget.h"
#include "ui_legacy_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QListView>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <core/misc/schedule_task.h>
#include <nx_ec/data/api_camera_attributes_data.h>

#include <camera/fps_calculator.h>

#include <licensing/license.h>

#include <text/time_strings.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/common/utils/checkbox_utils.h>
#include <nx/client/desktop/common/utils/aligner.h>
#include <nx/client/desktop/common/utils/stream_quality_strings.h>

#include <ui/common/palette.h>
#include <ui/common/read_only.h>
#include <ui/dialogs/resource_selection_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/style/custom_style.h>
#include <ui/style/globals.h>
#include <ui/style/skin.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/widgets/common/snapped_scrollbar.h>
#include <ui/widgets/properties/legacy_archive_length_widget.h>
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/math/math.h>
#include <utils/license_usage_helper.h>

#include "export_schedule_resource_selection_dialog_delegate.h"
#include <utils/camera/camera_bitrate_calculator.h>
#include <nx/utils/algorithm/same.h>

using boost::algorithm::all_of;
using boost::algorithm::any_of;

namespace {

static constexpr float kKbpsInMbps = 1000.0f;

void setLayoutEnabled(QLayout* layout, bool enabled)
{
    const auto count = layout->count();
    for (int i = 0; i != count; ++i)
    {
        const auto layoutItem = layout->itemAt(i);
        if (const auto widget = layoutItem->widget())
            widget->setEnabled(enabled);
        if (const auto childLayout = layoutItem->layout())
            setLayoutEnabled(childLayout, enabled);
    }
}

template<class InputWidget>
qreal normalizedValue(const InputWidget* widget)
{
    return qreal(widget->value() - widget->minimum()) / (widget->maximum() - widget->minimum());
}

template<class InputWidget>
void setNormalizedValue(InputWidget* widget, qreal fraction)
{
    widget->setValue(widget->minimum() + fraction * (widget->maximum() - widget->minimum()));
}

static const int kRecordingTypeLabelFontSize = 12;
static const int kRecordingTypeLabelFontWeight = QFont::DemiBold;

} // namespace

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

LegacyCameraScheduleWidget::LegacyCameraScheduleWidget(QWidget* parent, bool snapScrollbarToParent):
    base_type(parent),
    QnWorkbenchContextAware(parent, InitializationMode::lazy),
    ui(new Ui::LegacyCameraScheduleWidget),
    paintFunctions(new SchedulePaintFunctions())
{
    ui->setupUi(this);
    ui->recordBeforeSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    ui->recordAfterSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));

    NX_ASSERT(parent);
    QWidget* scrollbarOwner = snapScrollbarToParent
        ? window()
        : this;
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(scrollbarOwner);
    ui->scrollArea->setVerticalScrollBar(scrollBar->proxyScrollBar());
    scrollBar->setUseMaximumSpace(true);

    ui->enableRecordingCheckBox->setProperty(style::Properties::kCheckBoxAsButton, true);
    ui->enableRecordingCheckBox->setForegroundRole(QPalette::ButtonText);

    // Quality combo box contains Qn::StreamQuality constants as user data for normal visible items
    // (Low, Medium, High, Best), and those constants + Qn::StreamQualityCount for invisible items
    // with asterisks (Low*, Medium*, High*, Best*).
    const auto addQualityItem =
        [this](Qn::StreamQuality quality)
        {
            const auto text = toDisplayString(quality);
            ui->qualityComboBox->addItem(text, quality);
            const auto index = ui->qualityComboBox->count();
            ui->qualityComboBox->addItem(text + lit(" *"), Qn::StreamQualityCount + quality);
            static_cast<QListView*>(ui->qualityComboBox->view())->setRowHidden(index, true);
        };

    addQualityItem(Qn::QualityLow);
    addQualityItem(Qn::QualityNormal);
    addQualityItem(Qn::QualityHigh);
    addQualityItem(Qn::QualityHighest);
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(Qn::QualityHigh));

    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    updateColors();

    QnCamLicenseUsageHelper helper(commonModule());
    ui->licensesUsageWidget->init(&helper);

    CheckboxUtils::autoClearTristate(ui->enableRecordingCheckBox);

    QFont labelFont;
    labelFont.setPixelSize(kRecordingTypeLabelFontSize);
    labelFont.setWeight(kRecordingTypeLabelFontWeight);

    for (auto label : {ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord})
    {
        label->setFont(labelFont);
        label->setProperty(style::Properties::kDontPolishFontProperty, true);
    }

    auto notifyAboutScheduleEnabledChanged =
        [this](int state)
        {
            if (m_updating)
                return;

            updateAlert(EnabledChange);
            emit scheduleEnabledChanged(state);
        };

    auto notifyAboutArchiveRangeChanged =
        [this]
        {
            if (!m_updating)
                emit hasChangesChanged();
        };

    auto handleCellValuesChanged =
        [this]()
        {
            if (m_updating)
                return;

            updateAlert(ScheduleChange);
            emit hasChangesChanged();
        };

    connect(ui->gridWidget, &QnScheduleGridWidget::cellValuesChanged,
        this, handleCellValuesChanged);
    connect(ui->recordAlwaysButton, &QToolButton::toggled, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionButton, &QToolButton::toggled, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->bitrateSpinBox, QnSpinboxDoubleValueChanged, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &LegacyCameraScheduleWidget::updateMaxFpsValue);
    connect(ui->noRecordButton, &QToolButton::toggled, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &LegacyCameraScheduleWidget::updateGridParams);
    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged, this,
        &LegacyCameraScheduleWidget::updateGridParams);

    connect(ui->recordBeforeSpinBox, QnSpinboxIntValueChanged, this,
        &LegacyCameraScheduleWidget::hasChangesChanged);
    connect(ui->recordAfterSpinBox, QnSpinboxIntValueChanged, this,
        &LegacyCameraScheduleWidget::hasChangesChanged);

    connect(ui->licensesButton, &QPushButton::clicked, this,
        &LegacyCameraScheduleWidget::at_licensesButton_clicked);
    connect(ui->displayQualityCheckBox, &QCheckBox::stateChanged, this,
        &LegacyCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged);
    connect(ui->displayFpsCheckBox, &QCheckBox::stateChanged, this,
        &LegacyCameraScheduleWidget::at_displayFpsCheckBox_stateChanged);

    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &LegacyCameraScheduleWidget::updateLicensesLabelText);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &LegacyCameraScheduleWidget::updateGridEnabledState);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        notifyAboutScheduleEnabledChanged);

    connect(ui->gridWidget, &QnScheduleGridWidget::cellActivated, this,
        &LegacyCameraScheduleWidget::at_gridWidget_cellActivated);

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        &LegacyCameraScheduleWidget::at_exportScheduleButton_clicked);

    connect(ui->archiveLengthWidget, &QnArchiveLengthWidget::alertChanged, this,
        &nx::client::desktop::LegacyCameraScheduleWidget::alert);
    connect(ui->archiveLengthWidget, &QnArchiveLengthWidget::changed, this,
        notifyAboutArchiveRangeChanged);

    ui->exportWarningLabel->setVisible(false);

    installEventHandler({ ui->recordMotionButton, ui->recordMotionPlusLQButton },
        QEvent::MouseButtonRelease, this, &LegacyCameraScheduleWidget::at_releaseSignalizer_activated);

    updateMotionButtons();

    auto updateLicensesIfNeeded =
        [this]
        {
            if (!isVisible())
                return;
            updateLicensesLabelText();
        };

    auto camerasUsageWatcher = new QnCamLicenseUsageWatcher(commonModule(), this);
    connect(camerasUsageWatcher, &QnLicenseUsageWatcher::licenseUsageChanged, this,
        updateLicensesIfNeeded);

    connect(ui->advancedSettingsButton, &QPushButton::clicked, this,
        [this]() { setAdvancedSettingsVisible(!m_advancedSettingsVisible); });

    auto aligner = new Aligner(this);
    aligner->addWidgets({ ui->fpsLabel, ui->qualityLabel, ui->bitrateLabel });

    // Reset group box bottom margin to zero. Sub-widget margins defined in the ui-file rely on it.
    ui->settingsGroupBox->ensurePolished();
    auto margins = ui->settingsGroupBox->contentsMargins();
    margins.setBottom(0);
    ui->settingsGroupBox->setContentsMargins(margins);

    ui->bitrateSpinBox->setSuffix(lit(" ") + tr("Mbit/s"));

    ui->bitrateSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    // Sync bitrate spin box and quality combo box with bitrate slider changes.
    connect(ui->bitrateSlider, &QSlider::valueChanged,
        this, &LegacyCameraScheduleWidget::bitrateSliderChanged);

    // Sync bitrate slider and quality combo box with bitrate spin box changes.
    connect(ui->bitrateSpinBox, QnSpinboxDoubleValueChanged,
        this, &LegacyCameraScheduleWidget::bitrateSpinBoxChanged);

    // Sync bitrate with quality changes.
    syncBitrateWithQuality();
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged,
        this, &LegacyCameraScheduleWidget::syncBitrateWithQuality);

    // Sync bitrate with fps changes.
    syncBitrateWithFps();
    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged,
        this, &LegacyCameraScheduleWidget::syncBitrateWithFps);

    setAdvancedSettingsVisible(false);

    retranslateUi();
}

LegacyCameraScheduleWidget::~LegacyCameraScheduleWidget() = default;

void LegacyCameraScheduleWidget::setMotionDetectionAllowed(bool value)
{
    if (m_motionDetectionAllowed == value)
        return;

    m_motionDetectionAllowed = value;

    updateMotionAvailable();
    updateMaxFPS();
}

void LegacyCameraScheduleWidget::syncQualityWithBitrate()
{
    if (!m_advancedSettingsSupported)
        return;

    const auto quality = qualityForBitrate(ui->bitrateSpinBox->value());
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(quality.first
        + (quality.second ? 0 : Qn::StreamQualityCount)));
}

void LegacyCameraScheduleWidget::syncBitrateWithQuality()
{
    if (m_bitrateUpdating || !m_advancedSettingsSupported)
        return;

    ui->bitrateSpinBox->setValue(bitrateForQuality(currentQualityApproximation()));
}

void LegacyCameraScheduleWidget::syncBitrateWithFps()
{
    if (!m_advancedSettingsSupported)
        return;

    const auto minBitrate = bitrateForQuality(Qn::QualityLowest);
    const auto maxBitrate = bitrateForQuality(Qn::QualityHighest);

    if (isCurrentBitrateCustom())
    {
        // TODO: #vkutin Maybe should lock value normalized between two qualities.
        // Currently it's normalized between minimum and maximum bitrate.

        // Lock normalized bitrate. Use value from slider to avoid its jerking by 1px.
        const auto normalizedBitrate = normalizedValue(ui->bitrateSlider);
        ui->bitrateSpinBox->setRange(minBitrate, maxBitrate);
        setNormalizedValue(ui->bitrateSpinBox, normalizedBitrate);
        setNormalizedValue(ui->bitrateSlider, normalizedBitrate);
    }
    else
    {
        // Lock quality.
        const auto quality = currentQualityApproximation();
        ui->bitrateSpinBox->setRange(minBitrate, maxBitrate);
        ui->bitrateSpinBox->setValue(bitrateForQuality(quality));
        const auto normalizedBitrate = normalizedValue(ui->bitrateSpinBox);
        setNormalizedValue(ui->bitrateSlider, normalizedBitrate);

        // Force quality (in case several qualities have the same rounded bitrate value).
        QScopedValueRollback<bool> updateRollback(m_bitrateUpdating, true);
        ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(quality));
    }
}

void LegacyCameraScheduleWidget::bitrateSpinBoxChanged()
{
    if (m_bitrateUpdating)
        return;

    QScopedValueRollback<bool> updateRollback(m_bitrateUpdating, true);
    setNormalizedValue(ui->bitrateSlider, normalizedValue(ui->bitrateSpinBox));
    syncQualityWithBitrate();
}

void LegacyCameraScheduleWidget::bitrateSliderChanged()
{
    if (m_bitrateUpdating)
        return;

    QScopedValueRollback<bool> updateRollback(m_bitrateUpdating, true);
    setNormalizedValue(ui->bitrateSpinBox, normalizedValue(ui->bitrateSlider));
    syncQualityWithBitrate();
}

void LegacyCameraScheduleWidget::retranslateUi()
{
    ui->retranslateUi(this);

    ui->scheduleGridGroupBox->setTitle(lit("%1\t(%2)").arg(
        tr("Recording Schedule")).arg(
        tr("based on server time")));
}

void LegacyCameraScheduleWidget::afterContextInitialized()
{
    connect(
        context()->instance<QnWorkbenchPanicWatcher>(),
        &QnWorkbenchPanicWatcher::panicModeChanged,
        this,
        &LegacyCameraScheduleWidget::updatePanicLabelText);
    updatePanicLabelText();

    connect(
        context(),
        &QnWorkbenchContext::userChanged,
        this,
        &LegacyCameraScheduleWidget::updateLicensesButtonVisible);
    updateLicensesButtonVisible();
}

void LegacyCameraScheduleWidget::setReadOnlyInternal(bool readOnly)
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
    setReadOnly(ui->enableRecordingCheckBox, readOnly);
    setReadOnly(ui->gridWidget, readOnly);
    setReadOnly(ui->exportScheduleButton, readOnly);
    setReadOnly(ui->exportWarningLabel, readOnly);
    setReadOnly(ui->archiveLengthWidget, readOnly);
}

const QnVirtualCameraResourceList &LegacyCameraScheduleWidget::cameras() const
{
    return m_cameras;
}

void LegacyCameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    for (const auto& camera: m_cameras)
    {
        camera->disconnect(this);
    }

    m_cameras = cameras;

    for (const auto& camera: m_cameras)
    {
        connect(camera, &QnSecurityCamResource::resourceChanged,
            this, &LegacyCameraScheduleWidget::at_cameraResourceChanged, Qt::QueuedConnection);
        connect(camera, &QnResource::propertyChanged, this,
            [this](const QnResourcePtr& resource, const QString& propertyName)
            {
                if (propertyName == nx::media::kCameraMediaCapabilityParamName
                    || propertyName == Qn::CAMERA_MEDIA_STREAM_LIST_PARAM_NAME)
                {
                    syncBitrateWithFps();
                }
            });
    }
}

bool LegacyCameraScheduleWidget::hasChanges() const
{
    return false;
}

void LegacyCameraScheduleWidget::loadDataToUi()
{
    QScopedValueRollback<bool> updateRollback(m_updating, true);

    updateMaxFPS();
    updateMotionAvailable();
    updateRecordingParamsAvailable();

    m_advancedSettingsSupported = m_cameras.size() == 1;

    ui->advancedSettingsButton->setVisible(m_advancedSettingsSupported);
    ui->advancedSettingsWidget->setVisible(m_advancedSettingsSupported && m_advancedSettingsVisible);
    ui->settingsGroupBox->layout()->activate();

    if (!m_advancedSettingsSupported && isCurrentBitrateCustom())
    {
        ui->qualityComboBox->setCurrentIndex(
            ui->qualityComboBox->findData(currentQualityApproximation()));
    }

    int recordBeforeMotionSec = ec2::kDefaultRecordBeforeMotionSec;
    if (!utils::algorithm::same(m_cameras.cbegin(), m_cameras.cend(),
        [](const auto& camera) { return camera->recordBeforeMotionSec(); }, &recordBeforeMotionSec))
    {
        recordBeforeMotionSec = ec2::kDefaultRecordBeforeMotionSec;
    }
    ui->recordBeforeSpinBox->setValue(recordBeforeMotionSec);

    int recordAfterMotionSec = ec2::kDefaultRecordAfterMotionSec;
    if (!utils::algorithm::same(m_cameras.cbegin(), m_cameras.cend(),
        [](const auto& camera) { return camera->recordBeforeMotionSec(); }, &recordAfterMotionSec))
    {
        recordAfterMotionSec = ec2::kDefaultRecordAfterMotionSec;
    }
    ui->recordAfterSpinBox->setValue(recordAfterMotionSec);

    if (m_cameras.isEmpty())
    {
        setScheduleTasks(QnScheduleTaskList());
    }
    else
    {
        bool isScheduleEqual = true;
        bool isFpsValid = true;
        auto scheduleTasksData = m_cameras.front()->getScheduleTasks();

        for (const auto& camera: m_cameras)
        {
            QnScheduleTaskList cameraScheduleTasksData;
            for (const auto& scheduleTask : camera->getScheduleTasks())
            {
                cameraScheduleTasksData << scheduleTask;

                switch (scheduleTask.recordingType)
                {
                    case Qn::RT_Never:
                        continue;
                    case Qn::RT_MotionAndLowQuality:
                        isFpsValid &= scheduleTask.fps <= m_maxDualStreamingFps;
                        break;
                    case Qn::RT_Always:
                    case Qn::RT_MotionOnly:
                        isFpsValid &= scheduleTask.fps <= m_maxFps;
                        break;
                    default:
                        break;
                }
            }

            if (!isScheduleEqual || cameraScheduleTasksData != scheduleTasksData)
            {
                isScheduleEqual = false;
                break;
            }
        }

        if (isScheduleEqual)
            setScheduleTasks(m_cameras.front()->getScheduleTasks());
        else
            setScheduleTasks(QnScheduleTaskList());
        ui->gridWidget->setActive(isScheduleEqual && isFpsValid);
    }

    int currentMaxFps = getGridMaxFps();
    if (currentMaxFps > 0)
        setFps(currentMaxFps);
    else if (!m_cameras.isEmpty())
        setFps(m_cameras.first()->getMaxFps());
    syncBitrateWithFps();

    ui->archiveLengthWidget->updateFromResources(m_cameras);

    updateScheduleEnabled();
    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
    updateGridParams();
    setScheduleAlert(QString());

    retranslateUi();
}

void LegacyCameraScheduleWidget::applyChanges()
{
    bool scheduleChanged = ui->gridWidget->isActive();

    ui->archiveLengthWidget->submitToResources(m_cameras);

    QnScheduleTaskList scheduleTasks = this->scheduleTasks();

    bool enableRecording = isScheduleEnabled();
    bool canChangeRecording = !enableRecording || canEnableRecording();

    for (const auto& camera: m_cameras)
    {
        if (canChangeRecording)
            camera->setLicenseUsed(enableRecording);

        if (scheduleChanged && !camera->isDtsBased())
            camera->setScheduleTasks(scheduleTasks);

        // FIXME: #GDM This way we are always applying changes!
        camera->setRecordBeforeMotionSec(ui->recordBeforeSpinBox->value());
        camera->setRecordAfterMotionSec(ui->recordBeforeSpinBox->value());
    }

    if (!canChangeRecording)
    {
        QSignalBlocker blocker(ui->enableRecordingCheckBox);
        updateScheduleEnabled();
        setScheduleAlert(QString());
        updateLicensesLabelText();
    }
}

void LegacyCameraScheduleWidget::updateScheduleEnabled()
{
    int enabledCount = 0, disabledCount = 0;
    for (const auto &camera : m_cameras)
        (camera->isLicenseUsed() ? enabledCount : disabledCount)++;

    CheckboxUtils::setupTristateCheckbox(ui->enableRecordingCheckBox,
        enabledCount == 0 || disabledCount == 0, enabledCount > 0);
}

void LegacyCameraScheduleWidget::updateMaxFPS()
{
    QPair<int, int> fpsLimits = Qn::calculateMaxFps(m_cameras, m_motionDetectionAllowed);
    int maxFps = fpsLimits.first;
    int maxDualStreamingFps = fpsLimits.second;

    /* Silently ignoring invalid input is OK here. */
    if (maxFps < ui->fpsSpinBox->minimum())
        maxFps = ui->fpsSpinBox->minimum();
    if (maxDualStreamingFps < ui->fpsSpinBox->minimum())
        maxDualStreamingFps = ui->fpsSpinBox->minimum();

    if (m_maxFps != maxFps)
    {
        m_maxFps = maxFps;
        int currentMaxFps = getGridMaxFps();
        if (currentMaxFps > m_maxFps)
            emit hasChangesChanged();
    }

    if (m_maxDualStreamingFps != maxDualStreamingFps)
    {
        m_maxDualStreamingFps = maxDualStreamingFps;
        int currentMaxDualStreamingFps = getGridMaxFps(true);
        if (currentMaxDualStreamingFps > m_maxDualStreamingFps)
            emit hasChangesChanged();
    }

    updateMaxFpsValue(ui->recordMotionPlusLQButton->isChecked());
    ui->gridWidget->setMaxFps(m_maxFps, m_maxDualStreamingFps);
}

// Quality combo box contains Qn::StreamQuality constants as user data for normal visible items
// (Low, Medium, High, Best), and those constants + Qn::StreamQualityCount for invisible items
// with asterisks (Low*, Medium*, High*, Best*).
Qn::StreamQuality LegacyCameraScheduleWidget::currentQualityApproximation() const
{
    auto value = ui->qualityComboBox->currentData().toInt();
    if (value >= Qn::StreamQualityCount)
        value -= Qn::StreamQualityCount;
    return static_cast<Qn::StreamQuality>(value);
}

bool LegacyCameraScheduleWidget::isCurrentBitrateCustom() const
{
    return ui->qualityComboBox->currentData().toInt() >= Qn::StreamQualityCount;
}

void LegacyCameraScheduleWidget::setAdvancedSettingsVisible(bool value)
{
    const auto text = value ? tr("Less Settings") : tr("More Settings");
    const auto icon = qnSkin->icon(value ? lit("text_buttons/collapse.png") : lit("text_buttons/expand.png"));
    ui->advancedSettingsButton->setText(text);
    ui->advancedSettingsButton->setIcon(icon);
    ui->advancedSettingsWidget->setVisible(value);
    ui->settingsGroupBox->layout()->activate();
    m_advancedSettingsVisible = value;
}

QPair<Qn::StreamQuality, bool> LegacyCameraScheduleWidget::qualityForBitrate(qreal bitrateMbps) const
{
    static constexpr auto kMinQuality = Qn::QualityLow;
    static constexpr auto kMaxQuality = Qn::QualityHighest;

    auto current = kMinQuality;
    auto currentBr = bitrateForQuality(current);

    for (int i = current + 1; i <= kMaxQuality; ++i)
    {
        const auto next = Qn::StreamQuality(i);
        const auto nextBr = bitrateForQuality(next);

        if (bitrateMbps < (currentBr + nextBr) * 0.5)
            break;

        currentBr = nextBr;
        current = next;
    }

    return {current, qFuzzyIsNull(bitrateMbps - currentBr)};
}

qreal LegacyCameraScheduleWidget::bitrateForQuality(Qn::StreamQuality quality) const
{
    if (!m_advancedSettingsSupported)
        return 0.0;

    if (m_cameras.size() != 1)
    {
        NX_ASSERT(false, Q_FUNC_INFO);
        return 0.0;
    }

    return core::CameraBitrateCalculator::getBitrateForQualityMbps(m_cameras.front(), quality,
        ui->fpsSpinBox->value(), ui->bitrateSpinBox->decimals());
}

void LegacyCameraScheduleWidget::updateMotionAvailable()
{
    using boost::algorithm::all_of;

    const bool available = m_motionDetectionAllowed
        && all_of(m_cameras,
            [](const QnVirtualCameraResourcePtr &camera)
            { return !camera->isDtsBased() && camera->hasMotion(); });

    if (m_motionAvailable == available)
        return;

    m_motionAvailable = available;

    updateMotionButtons();
    updateRecordSpinboxes();
}

void LegacyCameraScheduleWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->exportScheduleButton->setEnabled(enabled);
    ui->exportWarningLabel->setVisible(!enabled);
}

void LegacyCameraScheduleWidget::updatePanicLabelText()
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

QnScheduleTaskList LegacyCameraScheduleWidget::scheduleTasks() const
{
    QnScheduleTaskList tasks;
    if (!ui->gridWidget->isActive())
        return tasks;

    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        QnScheduleTask task;

        for (int col = 0; col < ui->gridWidget->columnCount();)
        {
            const QPoint cell(col, row);
            const QnScheduleGridWidget::CellParams params = ui->gridWidget->cellValue(cell);

            Qn::RecordingType recordType = params.recordingType;
            Qn::StreamQuality streamQuality = Qn::QualityHighest;
            int bitrateKbps = 0;

            if (recordType != Qn::RT_Never)
            {
                streamQuality = params.quality;
                if (m_advancedSettingsSupported)
                    bitrateKbps = qRound(params.bitrateMbps * kKbpsInMbps);
            }

            int fps = params.fps;
            if (fps == 0 && recordType != Qn::RT_Never)
                fps = 10;

            if (task.startTime == task.endTime)
            {
                // An invalid one; initialize.
                task.dayOfWeek = row + 1;
                task.startTime = col * 3600; //< In secs from start of day.
                task.endTime = (col + 1) * 3600; //< In secs from start of day.
                task.recordingType = recordType;
                task.streamQuality = streamQuality;
                task.bitrateKbps = bitrateKbps;
                task.fps = fps;
                ++col;
            }
            else if (task.recordingType == recordType
                && task.streamQuality == streamQuality
                && task.bitrateKbps == bitrateKbps
                && task.fps == fps)
            {
                task.endTime = (col + 1) * 3600; //< In secs from start of day.
                ++col;
            }
            else
            {
                tasks.append(task);
                task = QnScheduleTask();
            }
        }

        if (task.startTime != task.endTime)
            tasks.append(task);
    }

    return tasks;
}

void LegacyCameraScheduleWidget::setScheduleTasks(const QnScheduleTaskList& value)
{
    QSignalBlocker gridBlocker(ui->gridWidget);

    QnScheduleTaskList tasks = value;

    ui->gridWidget->resetCellValues();
    if (tasks.isEmpty())
    {
        for (int nDay = 1; nDay <= 7; ++nDay)
        {
            QnScheduleTask data;
            data.dayOfWeek = nDay;
            data.startTime = 0;
            data.endTime = 86400;
            tasks << data;
        }
    }

    for (const auto &task : tasks)
    {
        const int row = task.dayOfWeek - 1;
        Qn::StreamQuality quality = Qn::QualityNotDefined;
        qreal bitrateMbps = 0.0;

        if (task.recordingType != Qn::RT_Never)
        {
            switch (task.streamQuality)
            {
                case Qn::QualityLow:
                case Qn::QualityNormal:
                case Qn::QualityHigh:
                case Qn::QualityHighest:
                    quality = task.streamQuality;
                    bitrateMbps = task.bitrateKbps / kKbpsInMbps;
                    break;
                default:
                    qWarning("LegacyCameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.streamQuality);
                    break;
            }
        }

        for (int col = task.startTime / 3600; col < task.endTime / 3600; ++col)
        {
            const QPoint cell(col, row);
            QnScheduleGridWidget::CellParams params;
            params.recordingType = task.recordingType;
            params.quality = quality;
            params.fps = task.fps;
            params.bitrateMbps = m_advancedSettingsSupported ? bitrateMbps : 0.0;
            ui->gridWidget->setCellValue(cell, params);
        }
    }

    if (!m_updating)
        emit hasChangesChanged();
}

bool LegacyCameraScheduleWidget::canEnableRecording() const
{
    QnCamLicenseUsageHelper licenseHelper(commonModule());
    return licenseHelper.canEnableRecording(m_cameras);
}

void LegacyCameraScheduleWidget::updateScheduleTypeControls()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
    const auto labels =
        { ui->labelAlways, ui->labelMotionOnly, ui->labelMotionPlusLQ, ui->labelNoRecord };
    for (auto label: labels)
    {
        const auto button = qobject_cast<QAbstractButton*>(label->buddy());
        const QPalette::ColorRole foreground = button && button->isChecked() && recordingEnabled
            ? QPalette::Highlight
            : QPalette::WindowText;
        label->setForegroundRole(foreground);
    }
}

void LegacyCameraScheduleWidget::updateGridParams(bool pickedFromGrid)
{
    if (m_disableUpdateGridParams)
        return;

    Qn::RecordingType recordType = Qn::RT_Never;
    if (ui->recordAlwaysButton->isChecked())
        recordType = Qn::RT_Always;
    else if (ui->recordMotionButton->isChecked())
        recordType = Qn::RT_MotionOnly;
    else if (ui->noRecordButton->isChecked())
        recordType = Qn::RT_Never;
    else if (ui->recordMotionPlusLQButton->isChecked())
        recordType = Qn::RT_MotionAndLowQuality;
    else
        qWarning() << "LegacyCameraScheduleWidget::No record type is selected!";

    updateScheduleTypeControls();
    bool enabled = !ui->noRecordButton->isChecked();
    ui->fpsSpinBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->advancedSettingsWidget->setEnabled(enabled && m_recordingParamsAvailable);
    updateRecordSpinboxes();

    if (!(isReadOnly() && pickedFromGrid))
    {
        QnScheduleGridWidget::CellParams brush;
        brush.recordingType = recordType;

        if (ui->noRecordButton->isChecked())
        {
            brush.fps = 0;
            brush.quality = Qn::QualityNotDefined;
        }
        else
        {
            const bool customBitrate = m_advancedSettingsSupported && isCurrentBitrateCustom();
            brush.fps = ui->fpsSpinBox->value();
            brush.quality = currentQualityApproximation();
            brush.bitrateMbps = customBitrate ? ui->bitrateSpinBox->value() : 0.0;
        }

        ui->gridWidget->setBrush(brush);
    }

    // TODO: #GDM is it really needed here?
    updateMaxFPS();

    if (m_updating)
        return;

    updateAlert(CurrentParamsChange);
}

void LegacyCameraScheduleWidget::setFps(int value)
{
    ui->fpsSpinBox->setValue(value);
}

int LegacyCameraScheduleWidget::getGridMaxFps(bool motionPlusLqOnly)
{
    return ui->gridWidget->getMaxFps(motionPlusLqOnly);
}

void LegacyCameraScheduleWidget::setScheduleEnabled(bool enabled)
{
    ui->enableRecordingCheckBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool LegacyCameraScheduleWidget::isScheduleEnabled() const
{
    return ui->enableRecordingCheckBox->checkState() != Qt::Unchecked;
}

void LegacyCameraScheduleWidget::updateGridEnabledState()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
    ui->motionGroupBox->setEnabled(m_recordingParamsAvailable);
    setLayoutEnabled(ui->recordingScheduleLayout, recordingEnabled);
    setLayoutEnabled(ui->scheduleSettingsLayout, recordingEnabled);
    setLayoutEnabled(ui->bottomParametersLayout, recordingEnabled);
    updateScheduleTypeControls();
}

void LegacyCameraScheduleWidget::updateLicensesLabelText()
{
    QnCamLicenseUsageHelper helper(commonModule());

    switch (ui->enableRecordingCheckBox->checkState())
    {
        case Qt::Checked:
            helper.propose(m_cameras, true);
            break;
        case Qt::Unchecked:
            helper.propose(m_cameras, false);
            break;
        default:
            break;
    }
    ui->licensesUsageWidget->loadData(&helper);
}

void LegacyCameraScheduleWidget::updateLicensesButtonVisible()
{
    ui->licensesButton->setVisible(accessController()->hasGlobalPermission(Qn::GlobalAdminPermission));
}

void LegacyCameraScheduleWidget::updateRecordSpinboxes()
{
    ui->recordBeforeSpinBox->setEnabled(m_motionAvailable);
    ui->recordAfterSpinBox->setEnabled(m_motionAvailable);
}

void LegacyCameraScheduleWidget::at_cameraResourceChanged()
{
    updateMaxFPS();
    updateMotionButtons();
}

void LegacyCameraScheduleWidget::updateMotionButtons()
{
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    for (const auto &camera: m_cameras)
    {
        hasDualStreaming &= camera->hasDualStreaming();
        hasMotion &= camera->hasMotion();
    }

    bool enabled;

    enabled = m_motionAvailable && hasMotion;
    ui->recordMotionButton->setEnabled(enabled);
    ui->labelMotionOnly->setEnabled(enabled);

    enabled = m_motionAvailable && hasDualStreaming && hasMotion;
    ui->recordMotionPlusLQButton->setEnabled(enabled);
    ui->labelMotionPlusLQ->setEnabled(enabled);

    if (ui->recordMotionButton->isChecked() && !ui->recordMotionButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);
    if (ui->recordMotionPlusLQButton->isChecked() && !ui->recordMotionPlusLQButton->isEnabled())
        ui->recordAlwaysButton->setChecked(true);

    if (!m_motionAvailable)
    {
        for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
        {
            for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
            {
                const QPoint cell(col, row);
                auto params = ui->gridWidget->cellValue(cell);
                Qn::RecordingType recordType = params.recordingType;
                if (recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                {
                    params.recordingType = Qn::RT_Always;
                    ui->gridWidget->setCellValue(cell, params);
                }
            }
        }
    }
}

void LegacyCameraScheduleWidget::updateMaxFpsValue(bool motionPlusLqToggled)
{
    int maximum = motionPlusLqToggled ? m_maxDualStreamingFps : m_maxFps;

    /* This check is necessary because Qt doesn't do it and resets spinbox state: */
    if (maximum == ui->fpsSpinBox->maximum())
        return;

    ui->fpsSpinBox->setMaximum(maximum);
}

void LegacyCameraScheduleWidget::updateRecordingParamsAvailable()
{
    bool available = any_of(m_cameras,
        [](const QnVirtualCameraResourcePtr& camera)
        {
            return camera->hasVideo(0)
                && !camera->hasParam(Qn::NO_RECORDING_PARAMS_PARAM_NAME);
        });

    if (m_recordingParamsAvailable == available)
        return;

    m_recordingParamsAvailable = available;

    updateGridEnabledState();

    ui->gridWidget->setShowQuality(ui->displayQualityCheckBox->isChecked() && m_recordingParamsAvailable);
    ui->gridWidget->setShowFps(ui->displayFpsCheckBox->isChecked() && m_recordingParamsAvailable);

    ui->fpsSpinBox->setEnabled(m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(m_recordingParamsAvailable);
    ui->displayQualityCheckBox->setEnabled(m_recordingParamsAvailable);
    ui->displayFpsCheckBox->setEnabled(m_recordingParamsAvailable);
}

void LegacyCameraScheduleWidget::updateColors()
{
    ui->recordAlwaysButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RT_Always));
    ui->recordMotionButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RT_MotionOnly));
    ui->recordMotionPlusLQButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RT_MotionAndLowQuality));
    ui->noRecordButton->setCustomPaintFunction(
        paintFunctions->paintCellFunction(Qn::RT_Never));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void LegacyCameraScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
{
    /* Called when a cell is Alt-clicked this handler fetches cell settings as current. */

    m_disableUpdateGridParams = true;
    const auto params = ui->gridWidget->cellValue(cell);
    switch (params.recordingType)
    {
        case Qn::RT_Always:
            ui->recordAlwaysButton->setChecked(true);
            break;
        case Qn::RT_MotionOnly:
            ui->recordMotionButton->setChecked(true);
            break;
        case Qn::RT_MotionAndLowQuality:
            ui->recordMotionPlusLQButton->setChecked(true);
            break;
        default:
            ui->noRecordButton->setChecked(true);
            break;
    }

    if (params.recordingType != Qn::RT_Never)
    {
        ui->fpsSpinBox->setValue(params.fps);
        if (qFuzzyIsNull(params.bitrateMbps) || !m_advancedSettingsSupported)
            ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(params.quality));
        else
            ui->bitrateSpinBox->setValue(params.bitrateMbps);
    }

    m_disableUpdateGridParams = false;
    updateGridParams(true);
}

void LegacyCameraScheduleWidget::at_displayQualiteCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowQuality(state && m_recordingParamsAvailable);
}

void LegacyCameraScheduleWidget::at_displayFpsCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowFps(state && m_recordingParamsAvailable);
}

void LegacyCameraScheduleWidget::at_licensesButton_clicked()
{
    menu()->trigger(action::PreferencesLicensesTabAction);
}

void LegacyCameraScheduleWidget::at_releaseSignalizer_activated(QObject *target)
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
    else /* One camera */
    {
        NX_ASSERT(m_cameras.size() == 1, Q_FUNC_INFO, "Following options are valid only for singular camera");
        QnVirtualCameraResourcePtr camera = m_cameras.first();

        // TODO: #GDM #Common duplicate code.
        bool hasDualStreaming = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasDualStreaming(); });
        bool hasMotion = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasMotion(); });

        if (hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming not supported for this camera"));
        }
        else if (!hasMotion && !hasDualStreaming)
        {
            QnMessageBox::warning(this, tr("Dual-Streaming and motion detection not supported for this camera"));
        }
        else /* Has dual streaming but not motion */
        {
            QnMessageBox::warning(this,
                tr("Motion detection disabled"),
                tr("To enable or adjust it, go to the \"Motion\" tab in Camera Settings."));
        }
    }
}

void LegacyCameraScheduleWidget::at_exportScheduleButton_clicked()
{
    if (m_cameras.size() > 1)
    {
        NX_EXPECT(false, Q_FUNC_INFO);
        return;
    }

    bool recordingEnabled = ui->enableRecordingCheckBox->checkState() == Qt::Checked;
    bool motionUsed = recordingEnabled && hasMotionOnGrid();
    bool dualStreamingUsed = motionUsed && hasDualStreamingMotionOnGrid();
    bool hasVideo = boost::algorithm::all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });

    QScopedPointer<QnResourceSelectionDialog> dialog(
        new QnResourceSelectionDialog(QnResourceSelectionDialog::Filter::cameras, this));
    auto dialogDelegate = new ExportScheduleResourceSelectionDialogDelegate(this, recordingEnabled, motionUsed, dualStreamingUsed, hasVideo);
    dialog->setDelegate(dialogDelegate);

    QSet<QnUuid> ids;
    for (auto camera: m_cameras)
        ids << camera->getId();
    dialog->setSelectedResources(ids);
    setHelpTopic(dialog.data(), Qn::CameraSettings_Recording_Export_Help);
    if (!dialog->exec())
        return;

    const bool copyArchiveLength = dialogDelegate->doCopyArchiveLength();

    auto applyChanges =
        [this, sourceCamera = m_cameras.front(), copyArchiveLength, recordingEnabled]
            (const QnVirtualCameraResourcePtr &camera)
        {
            if (copyArchiveLength)
                ui->archiveLengthWidget->submitToResources({camera});

            camera->setLicenseUsed(recordingEnabled);
            int maxFps = camera->getMaxFps();

            // TODO: #GDM #Common ask: what about constant MIN_SECOND_STREAM_FPS moving out of this module
            // or just use camera->reservedSecondStreamFps();

            int decreaseAlways = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing && camera->getMotionType() == Qn::MT_SoftwareGrid)
                decreaseAlways = QnLiveStreamParams::kMinSecondStreamFps;

            int decreaseIfMotionPlusLQ = 0;
            if (camera->streamFpsSharingMethod() == Qn::BasicFpsSharing)
                decreaseIfMotionPlusLQ = QnLiveStreamParams::kMinSecondStreamFps;

            QnScheduleTaskList tasks;
            for (auto task: scheduleTasks())
            {
                if (task.recordingType == Qn::RT_MotionAndLowQuality)
                    task.fps = qMin(task.fps, maxFps - decreaseIfMotionPlusLQ);
                else
                    task.fps = qMin(task.fps, maxFps - decreaseAlways);

                if (const auto bitrate = task.bitrateKbps) // Try to calculate new custom bitrate
                {
                    // Target camera supports custom bitrate
                    const auto normalBitrate = core::CameraBitrateCalculator::getBitrateForQualityMbps(
                        sourceCamera,
                        task.streamQuality,
                        task.fps,
                        ui->bitrateSpinBox->decimals());
                    const auto bitrateAspect = (bitrate - normalBitrate) / normalBitrate;

                    const auto targetNormalBitrate = core::CameraBitrateCalculator::getBitrateForQualityMbps(
                        camera,
                        task.streamQuality,
                        task.fps,
                        ui->bitrateSpinBox->decimals());
                    const auto targetBitrate = targetNormalBitrate * bitrateAspect;
                    task.bitrateKbps = targetBitrate;
                }
                tasks.append(task);
            }

            camera->setRecordBeforeMotionSec(sourceCamera->recordBeforeMotionSec());
            camera->setRecordAfterMotionSec(sourceCamera->recordAfterMotionSec());
            camera->setScheduleTasks(tasks);
        };

    auto selectedCameras = resourcePool()->getResourcesByIds<QnVirtualCameraResource>(
        dialog->selectedResources());
    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
    updateLicensesLabelText();
}

bool LegacyCameraScheduleWidget::hasMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            Qn::RecordingType recordType = ui->gridWidget->cellValue(cell).recordingType;
            if (recordType == Qn::RT_MotionOnly || recordType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

bool LegacyCameraScheduleWidget::hasDualStreamingMotionOnGrid() const
{
    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        for (int col = 0; col < ui->gridWidget->columnCount(); ++col)
        {
            const QPoint cell(col, row);
            if (ui->gridWidget->cellValue(cell).recordingType == Qn::RT_MotionAndLowQuality)
                return true;
        }
    }
    return false;
}

void LegacyCameraScheduleWidget::setScheduleAlert(const QString& scheduleAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_scheduleAlert = scheduleAlert;
    emit alert(m_scheduleAlert);
}

void LegacyCameraScheduleWidget::setArchiveLengthAlert(const QString& archiveLengthAlert)
{
    if (!m_scheduleAlert.isEmpty())
        emit alert(m_scheduleAlert);
    else
        emit alert(ui->archiveLengthWidget->alert());
}

void LegacyCameraScheduleWidget::updateAlert(AlertReason when)
{
    /* License check and alert: */
    const auto checkCanEnableRecording =
        [this]() -> bool
        {
            if (canEnableRecording())
                return true;

            switch (ui->enableRecordingCheckBox->checkState())
            {
                case Qt::Unchecked:
                case Qt::PartiallyChecked:
                    setScheduleAlert(tr("Not enough licenses to enable recording"));
                    break;

                case Qt::Checked:
                    setScheduleAlert(tr("License limit exceeded, recording will not be enabled."));
                    break;

                default:
                    break;
            }

            return false;
        };

    switch (when)
    {
        /* Current "brush" was changed (mode, fps, quality): */
        case CurrentParamsChange:
        {
            if (checkCanEnableRecording() && !isRecordingScheduled())
                setScheduleAlert(tr("Select areas on the schedule to apply chosen parameters to."));
            break;
        }

        /* Some cell(s) in the schedule were changed: */
        case ScheduleChange:
        {
            if (isRecordingScheduled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isScheduleEnabled())
                {
                    setScheduleAlert(tr("Turn on selector at the top of the window to enable recording."));
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }

        /* Recording was enabled or disabled: */
        case EnabledChange:
        {
            if (isScheduleEnabled())
            {
                if (!checkCanEnableRecording())
                    break;

                if (!isRecordingScheduled())
                {
                    setScheduleAlert(tr("Set recording parameters and select areas on the schedule grid to apply them to."));
                    break;
                }
            }

            setScheduleAlert(QString());
            break;
        }
    }
}

bool LegacyCameraScheduleWidget::isRecordingScheduled() const
{
    return any_of(scheduleTasks(),
        [](const QnScheduleTask& task) -> bool
        {
            return !task.isEmpty() && task.recordingType != Qn::RT_Never;
        });
}

} // namespace desktop
} // namespace client
} // namespace nx
