#include "camera_schedule_widget.h"
#include "ui_camera_schedule_widget.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QScopedValueRollback>
#include <QtWidgets/QListView>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resources_changes_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/device_dependent_strings.h>
#include <core/resource/media_server_resource.h>
#include <nx_ec/data/api_camera_attributes_data.h>

#include <camera/fps_calculator.h>

#include <licensing/license.h>

#include <text/time_strings.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/common/checkbox_utils.h>
#include <ui/common/aligner.h>
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
#include <ui/workbench/watchers/workbench_panic_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/event_processors.h>
#include <utils/math/color_transformations.h>
#include <utils/math/math.h>
#include <utils/license_usage_helper.h>

using boost::algorithm::all_of;
using boost::algorithm::any_of;

namespace {

static constexpr qreal kKbpsInMbps = 1000.0;

qreal getBitrateForQuality(
    const QnVirtualCameraResourcePtr& camera,
    Qn::StreamQuality quality,
    int fps,
    int decimals)
{
    const auto resolution = camera->defaultStream().getResolution();
    const auto bitrateMbps = camera->suggestBitrateForQualityKbps(quality, resolution, fps)
        / kKbpsInMbps;
    const auto roundingStep = std::pow(0.1, decimals);
    return qRound(bitrateMbps, roundingStep);
}

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

class ExportScheduleResourceSelectionDialogDelegate: public QnResourceSelectionDialogDelegate
{
    Q_DECLARE_TR_FUNCTIONS(ExportScheduleResourceSelectionDialogDelegate);
    typedef QnResourceSelectionDialogDelegate base_type;
public:
    ExportScheduleResourceSelectionDialogDelegate(QWidget* parent,
        bool recordingEnabled,
        bool motionUsed,
        bool dualStreamingUsed,
        bool hasVideo)
        :
        base_type(parent),
        m_licensesLabel(NULL),
        m_motionLabel(NULL),
        m_dtsLabel(NULL),
        m_recordingEnabled(recordingEnabled),
        m_motionUsed(motionUsed),
        m_dualStreamingUsed(dualStreamingUsed),
        m_hasVideo(hasVideo)
    {
    }

    virtual ~ExportScheduleResourceSelectionDialogDelegate()
    {

    }

    bool doCopyArchiveLength() const
    {
        return m_archiveCheckbox->isChecked();
    }

    virtual void init(QWidget* parent) override
    {
        m_parentPalette = parent->palette();

        m_archiveCheckbox = new QCheckBox(parent);
        m_archiveCheckbox->setText(tr("Copy archive length settings"));
        parent->layout()->addWidget(m_archiveCheckbox);

        m_licensesLabel = new QLabel(parent);
        parent->layout()->addWidget(m_licensesLabel);

        auto addWarningLabel = [parent](const QString &text) -> QLabel*
        {
            auto label = new QLabel(text, parent);
            setWarningStyle(label);
            parent->layout()->addWidget(label);
            label->setVisible(false);
            return label;
        };

        m_motionLabel = addWarningLabel(tr("Schedule motion type is not supported by some cameras."));
        m_dtsLabel = addWarningLabel(tr("Recording cannot be enabled for some cameras."));
        m_noVideoLabel = addWarningLabel(tr("Schedule settings are not compatible with some devices."));
    }

    virtual bool validate(const QSet<QnUuid>& selected) override
    {
        auto cameras = resourcePool()->getResources<QnVirtualCameraResource>(selected);

        QnCamLicenseUsageHelper helper(cameras, m_recordingEnabled, commonModule());

        QPalette palette = m_parentPalette;
        bool licensesOk = helper.isValid();
        QString licenseUsage = helper.getProposedUsageMsg();
        if (!licensesOk)
        {
            setWarningStyle(&palette);
            licenseUsage += L'\n' + helper.getRequiredMsg();
        }
        m_licensesLabel->setText(licenseUsage);
        m_licensesLabel->setPalette(palette);

        bool motionOk = true;
        if (m_motionUsed)
        {
            foreach(const QnVirtualCameraResourcePtr &camera, cameras)
            {
                bool hasMotion = (camera->getMotionType() != Qn::MT_NoMotion);
                if (!hasMotion)
                {
                    motionOk = false;
                    break;
                }
                if (m_dualStreamingUsed && !camera->hasDualStreaming2())
                {
                    motionOk = false;
                    break;
                }
            }
        }
        m_motionLabel->setVisible(!motionOk);

        bool dtsCamPresent = any_of(cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->isDtsBased(); });
        m_dtsLabel->setVisible(dtsCamPresent);

        /* If source camera has no video, allow to copy only to cameras without video. */
        bool videoOk = m_hasVideo || !any_of(cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasVideo(0); });
        m_noVideoLabel->setVisible(!videoOk);

        return licensesOk && motionOk && !dtsCamPresent && videoOk;
    }

private:
    QLabel* m_licensesLabel;
    QLabel* m_motionLabel;
    QLabel* m_dtsLabel;
    QLabel* m_noVideoLabel;
    QCheckBox *m_archiveCheckbox;
    QPalette m_parentPalette;

    bool m_recordingEnabled;
    bool m_motionUsed;
    bool m_dualStreamingUsed;
    bool m_hasVideo;
};

static const int kDangerousMinArchiveDays = 5;
static const int kRecordingTypeLabelFontSize = 12;
static const int kRecordingTypeLabelFontWeight = QFont::DemiBold;
static const int kDefaultBeforeThresholdSec = 5;
static const int kDefaultAfterThresholdSec = 5;
static const int kRecordedDaysDontChange = std::numeric_limits<int>::max();

} // namespace

namespace nx {
namespace client {
namespace desktop {

using namespace ui;

CameraScheduleWidget::CameraScheduleWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent, true),
    ui(new Ui::CameraScheduleWidget),
    m_disableUpdateGridParams(false),
    m_motionAvailable(true),
    m_recordingParamsAvailable(true),
    m_readOnly(false),
    m_maxFps(0),
    m_maxDualStreamingFps(0),
    m_motionTypeOverride(Qn::MT_Default),
    m_updating(false)
{
    ui->setupUi(this);
    ui->recordBeforeSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));
    ui->recordAfterSpinBox->setSuffix(L' ' + QnTimeStrings::suffix(QnTimeStrings::Suffix::Seconds));

    NX_ASSERT(parent);
    QnSnappedScrollBar* scrollBar = new QnSnappedScrollBar(window());
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

    setHelpTopic(ui->archiveGroupBox, Qn::CameraSettings_Recording_ArchiveLength_Help);
    setHelpTopic(ui->exportScheduleButton, Qn::CameraSettings_Recording_Export_Help);

    // init buttons
    connect(ui->gridWidget, &QnScheduleGridWidget::colorsChanged, this,
        &CameraScheduleWidget::updateColors);
    updateColors();

    QnCamLicenseUsageHelper helper(commonModule());
    ui->licensesUsageWidget->init(&helper);

    CheckboxUtils::autoClearTristate(ui->enableRecordingCheckBox);
    CheckboxUtils::autoClearTristate(ui->checkBoxMinArchive);
    CheckboxUtils::autoClearTristate(ui->checkBoxMaxArchive);

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
                emit archiveRangeChanged();
        };

    auto handleCellValuesChanged =
        [this]()
        {
            if (m_updating)
                return;

            updateAlert(ScheduleChange);
            emit scheduleTasksChanged();
        };

    connect(ui->gridWidget, &QnScheduleGridWidget::cellValuesChanged,
        this, handleCellValuesChanged);
    connect(ui->recordAlwaysButton, &QToolButton::toggled, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionButton, &QToolButton::toggled, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->bitrateSpinBox, QnSpinboxDoubleValueChanged, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->recordMotionPlusLQButton, &QToolButton::toggled, this,
        &CameraScheduleWidget::updateMaxFpsValue);
    connect(ui->noRecordButton, &QToolButton::toggled, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged, this,
        &CameraScheduleWidget::updateGridParams);
    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged, this,
        &CameraScheduleWidget::updateGridParams);
    connect(this, &CameraScheduleWidget::scheduleTasksChanged, this,
        &CameraScheduleWidget::updateRecordSpinboxes);

    connect(ui->recordBeforeSpinBox, QnSpinboxIntValueChanged, this,
        &CameraScheduleWidget::recordingSettingsChanged);
    connect(ui->recordAfterSpinBox, QnSpinboxIntValueChanged, this,
        &CameraScheduleWidget::recordingSettingsChanged);

    connect(ui->licensesButton, &QPushButton::clicked, this,
        &CameraScheduleWidget::at_licensesButton_clicked);
    connect(ui->displayQualityCheckBox, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::at_displayQualiteCheckBox_stateChanged);
    connect(ui->displayFpsCheckBox, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::at_displayFpsCheckBox_stateChanged);

    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::updateLicensesLabelText);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::updateGridEnabledState);
    connect(ui->enableRecordingCheckBox, &QCheckBox::stateChanged, this,
        notifyAboutScheduleEnabledChanged);

    connect(ui->gridWidget, &QnScheduleGridWidget::cellActivated, this,
        &CameraScheduleWidget::at_gridWidget_cellActivated);

    connect(ui->checkBoxMinArchive, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMinArchive, &QCheckBox::stateChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->checkBoxMaxArchive, &QCheckBox::stateChanged, this,
        &CameraScheduleWidget::updateArchiveRangeEnabledState);
    connect(ui->checkBoxMaxArchive, &QCheckBox::stateChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged, this,
        &CameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMinDays, QnSpinboxIntValueChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged, this,
        &CameraScheduleWidget::validateArchiveLength);
    connect(ui->spinBoxMaxDays, QnSpinboxIntValueChanged, this,
        notifyAboutArchiveRangeChanged);

    connect(ui->exportScheduleButton, &QPushButton::clicked, this,
        &CameraScheduleWidget::at_exportScheduleButton_clicked);

    ui->exportWarningLabel->setVisible(false);

    installEventHandler({ ui->recordMotionButton, ui->recordMotionPlusLQButton },
        QEvent::MouseButtonRelease, this, &CameraScheduleWidget::at_releaseSignalizer_activated);

    installEventHandler(ui->gridWidget, QEvent::MouseButtonRelease,
        this, &CameraScheduleWidget::controlsChangesApplied);

    updateGridEnabledState();
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

    auto aligner = new QnAligner(this);
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
        this, &CameraScheduleWidget::bitrateSliderChanged);

    // Sync bitrate slider and quality combo box with bitrate spin box changes.
    connect(ui->bitrateSpinBox, QnSpinboxDoubleValueChanged,
        this, &CameraScheduleWidget::bitrateSpinBoxChanged);

    // Sync bitrate with quality changes.
    syncBitrateWithQuality();
    connect(ui->qualityComboBox, QnComboboxCurrentIndexChanged,
        this, &CameraScheduleWidget::syncBitrateWithQuality);

    // Sync bitrate with fps changes.
    syncBitrateWithFps();
    connect(ui->fpsSpinBox, QnSpinboxIntValueChanged,
        this, &CameraScheduleWidget::syncBitrateWithFps);

    setAdvancedSettingsVisible(false);

    retranslateUi();
}

CameraScheduleWidget::~CameraScheduleWidget()
{
}

void CameraScheduleWidget::syncQualityWithBitrate()
{
    if (!m_advancedSettingsSupported)
        return;

    const auto quality = qualityForBitrate(ui->bitrateSpinBox->value());
    ui->qualityComboBox->setCurrentIndex(ui->qualityComboBox->findData(quality.first
        + (quality.second ? 0 : Qn::StreamQualityCount)));
}

void CameraScheduleWidget::syncBitrateWithQuality()
{
    if (m_bitrateUpdating || !m_advancedSettingsSupported)
        return;

    ui->bitrateSpinBox->setValue(bitrateForQuality(currentQualityApproximation()));
}

void CameraScheduleWidget::syncBitrateWithFps()
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

void CameraScheduleWidget::bitrateSpinBoxChanged()
{
    if (m_bitrateUpdating)
        return;

    QScopedValueRollback<bool> updateRollback(m_bitrateUpdating, true);
    setNormalizedValue(ui->bitrateSlider, normalizedValue(ui->bitrateSpinBox));
    syncQualityWithBitrate();
}

void CameraScheduleWidget::bitrateSliderChanged()
{
    if (m_bitrateUpdating)
        return;

    QScopedValueRollback<bool> updateRollback(m_bitrateUpdating, true);
    setNormalizedValue(ui->bitrateSpinBox, normalizedValue(ui->bitrateSlider));
    syncQualityWithBitrate();
}

void CameraScheduleWidget::overrideMotionType(Qn::MotionType motionTypeOverride)
{
    if (m_motionTypeOverride == motionTypeOverride)
        return;

    m_motionTypeOverride = motionTypeOverride;

    updateMotionAvailable();
    updateMaxFPS();
}

void CameraScheduleWidget::retranslateUi()
{
    ui->retranslateUi(this);

    ui->scheduleGridGroupBox->setTitle(lit("%1\t(%2)").arg(
        tr("Recording Schedule")).arg(
        tr("based on server time")));
}

void CameraScheduleWidget::afterContextInitialized()
{
    connect(context()->instance<QnWorkbenchPanicWatcher>(), &QnWorkbenchPanicWatcher::panicModeChanged, this, &CameraScheduleWidget::updatePanicLabelText);
    connect(context(), &QnWorkbenchContext::userChanged, this, &CameraScheduleWidget::updateLicensesButtonVisible);
    updatePanicLabelText();
    updateLicensesButtonVisible();
}

bool CameraScheduleWidget::isReadOnly() const
{
    return m_readOnly;
}

void CameraScheduleWidget::setReadOnly(bool readOnly)
{
    if (m_readOnly == readOnly)
        return;

    using ::setReadOnly;
    setReadOnly(ui->recordAlwaysButton, readOnly);
    setReadOnly(ui->recordMotionButton, readOnly); // TODO: #GDM #Common this is not valid. Camera may not support HW motion, we need to check for this.
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

    setReadOnly(ui->checkBoxMaxArchive, readOnly);
    setReadOnly(ui->checkBoxMinArchive, readOnly);
    setReadOnly(ui->spinBoxMaxDays, readOnly);
    setReadOnly(ui->spinBoxMinDays, readOnly);
    m_readOnly = readOnly;
}

const QnVirtualCameraResourceList &CameraScheduleWidget::cameras() const
{
    return m_cameras;
}

void CameraScheduleWidget::setCameras(const QnVirtualCameraResourceList &cameras)
{
    if (m_cameras == cameras)
        return;

    for (const auto& camera: m_cameras)
    {
        disconnect(camera, &QnSecurityCamResource::resourceChanged,
            this, &CameraScheduleWidget::cameraResourceChanged);
    }

    m_cameras = cameras;

    for (const auto& camera: m_cameras)
    {
        connect(camera, &QnSecurityCamResource::resourceChanged,
            this, &CameraScheduleWidget::cameraResourceChanged, Qt::QueuedConnection);
    }
}

void CameraScheduleWidget::updateFromResources()
{
    QScopedValueRollback<bool> updateRollback(m_updating, true);

    updateMaxFPS();
    updateMotionAvailable();
    updateRecordingParamsAvailable();

    m_advancedSettingsSupported = m_cameras.size() == 1
        && !m_cameras.front()->cameraMediaCapability().isNull();

    ui->advancedSettingsButton->setVisible(m_advancedSettingsSupported);
    ui->advancedSettingsWidget->setVisible(m_advancedSettingsSupported && m_advancedSettingsVisible);
    ui->settingsGroupBox->layout()->activate();

    if (!m_advancedSettingsSupported && isCurrentBitrateCustom())
    {
        ui->qualityComboBox->setCurrentIndex(
            ui->qualityComboBox->findData(currentQualityApproximation()));
    }

    if (m_cameras.isEmpty())
    {
        setScheduleTasks(QnScheduleTaskList());
    }
    else
    {
        bool isScheduleEqual = true;
        QList<QnScheduleTask::Data> scheduleTasksData;
        for (const auto& scheduleTask : m_cameras.front()->getScheduleTasks())
            scheduleTasksData << scheduleTask.getData();

        for (const auto &camera : m_cameras)
        {
            QList<QnScheduleTask::Data> cameraScheduleTasksData;
            for (const auto& scheduleTask : camera->getScheduleTasks())
            {
                cameraScheduleTasksData << scheduleTask.getData();

                bool fpsValid = true;
                switch (scheduleTask.getRecordingType())
                {
                    case Qn::RT_Never:
                        continue;
                    case Qn::RT_MotionAndLowQuality:
                        fpsValid = scheduleTask.getFps() <= m_maxDualStreamingFps;
                        break;
                    case Qn::RT_Always:
                    case Qn::RT_MotionOnly:
                        fpsValid = scheduleTask.getFps() <= m_maxFps;
                        break;
                    default:
                        break;
                }

                if (!fpsValid)
                {
                    isScheduleEqual = false;
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
        ui->gridWidget->setActive(isScheduleEqual);
    }

    int currentMaxFps = getGridMaxFps();
    if (currentMaxFps > 0)
        setFps(currentMaxFps);
    else if (!m_cameras.isEmpty())
        setFps(m_cameras.first()->getMaxFps());
    syncBitrateWithFps();

    updateScheduleEnabled();
    updateMinDays();
    updateMaxDays();
    updatePanicLabelText();
    updateMotionButtons();
    updateLicensesLabelText();
    updateGridParams();
    setScheduleAlert(QString());
    retranslateUi();
}

void CameraScheduleWidget::submitToResources()
{
    int maxDays = maxRecordedDays();
    int minDays = minRecordedDays();

    bool scheduleChanged = ui->gridWidget->isActive();

    QnScheduleTaskList basicScheduleTasks;
    if (scheduleChanged)
    {
        for (const auto& scheduleTaskData : this->scheduleTasks())
            basicScheduleTasks.append(scheduleTaskData);
    }
    updateRecordThresholds(basicScheduleTasks);

    bool enableRecording = isScheduleEnabled();
    bool canChangeRecording = !enableRecording || canEnableRecording();

    for (const auto& camera: m_cameras)
    {
        if (maxDays != kRecordedDaysDontChange)
            camera->setMaxDays(maxDays);

        if (minDays != kRecordedDaysDontChange)
            camera->setMinDays(minDays);

        if (canChangeRecording)
        {
            camera->setLicenseUsed(enableRecording);
            camera->setScheduleDisabled(!enableRecording);
        }

        if (camera->isDtsBased())
            continue;

        QnScheduleTaskList scheduleTasks = basicScheduleTasks;
        if (!scheduleChanged)
        {
            scheduleTasks = camera->getScheduleTasks();
            updateRecordThresholds(scheduleTasks);
        }
        camera->setScheduleTasks(scheduleTasks);
    }

    if (!canChangeRecording)
    {
        QSignalBlocker blocker(ui->enableRecordingCheckBox);
        updateScheduleEnabled();
        setScheduleAlert(QString());
        updateLicensesLabelText();
    }
}

void CameraScheduleWidget::updateScheduleEnabled()
{
    int enabledCount = 0, disabledCount = 0;
    for (const auto &camera : m_cameras)
        (camera->isScheduleDisabled() ? disabledCount : enabledCount)++;

    CheckboxUtils::setupTristateCheckbox(ui->enableRecordingCheckBox,
        enabledCount == 0 || disabledCount == 0, enabledCount > 0);
}

void CameraScheduleWidget::updateMinDays()
{
    if (m_cameras.isEmpty())
        return;

    /* Any negative min days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMinDays = [](int d) { return d == 0 ? ec2::kDefaultMinArchiveDays : qAbs(d); };

    const int minDays = (*std::min_element(m_cameras.cbegin(), m_cameras.cend(),
        [calcMinDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r)
        {
            return calcMinDays(l->minDays()) < calcMinDays(r->minDays());
        }))->minDays();

    const bool isAuto = minDays <= 0;

    bool sameMinDays = boost::algorithm::all_of(m_cameras,
        [minDays, isAuto](const QnVirtualCameraResourcePtr &camera)
        {
            return isAuto
                ? camera->minDays() <= 0
                : camera->minDays() == minDays;
        });

    CheckboxUtils::setupTristateCheckbox(ui->checkBoxMinArchive, sameMinDays, isAuto);
    ui->spinBoxMinDays->setValue(calcMinDays(minDays));
}

void CameraScheduleWidget::updateMaxDays()
{
    if (m_cameras.isEmpty())
        return;

    /* Any negative max days value means 'auto'. Storing absolute value to keep previous one. */
    auto calcMaxDays = [](int d) { return d == 0 ? ec2::kDefaultMaxArchiveDays : qAbs(d); };

    const int maxDays =(*std::max_element(m_cameras.cbegin(), m_cameras.cend(),
        [calcMaxDays](const QnVirtualCameraResourcePtr &l, const QnVirtualCameraResourcePtr &r)
        {
            return calcMaxDays(l->maxDays()) < calcMaxDays(r->maxDays());
        }))->maxDays();

    const bool isAuto = maxDays <= 0;
    bool sameMaxDays = boost::algorithm::all_of(m_cameras,
        [maxDays, isAuto](const QnVirtualCameraResourcePtr &camera)
        {
        return isAuto
            ? camera->maxDays() <= 0
            : camera->maxDays() == maxDays;
        });

    CheckboxUtils::setupTristateCheckbox(ui->checkBoxMaxArchive, sameMaxDays, isAuto);
    ui->spinBoxMaxDays->setValue(calcMaxDays(maxDays));
}

void CameraScheduleWidget::updateMaxFPS()
{
    QPair<int, int> fpsLimits = Qn::calculateMaxFps(m_cameras, m_motionTypeOverride);
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
            emit scheduleTasksChanged();
    }

    if (m_maxDualStreamingFps != maxDualStreamingFps)
    {
        m_maxDualStreamingFps = maxDualStreamingFps;
        int currentMaxDualStreamingFps = getGridMaxFps(true);
        if (currentMaxDualStreamingFps > m_maxDualStreamingFps)
            emit scheduleTasksChanged();
    }

    updateMaxFpsValue(ui->recordMotionPlusLQButton->isChecked());
    ui->gridWidget->setMaxFps(m_maxFps, m_maxDualStreamingFps);
}

// Quality combo box contains Qn::StreamQuality constants as user data for normal visible items
// (Low, Medium, High, Best), and those constants + Qn::StreamQualityCount for invisible items
// with asterisks (Low*, Medium*, High*, Best*).
Qn::StreamQuality CameraScheduleWidget::currentQualityApproximation() const
{
    auto value = ui->qualityComboBox->currentData().toInt();
    if (value >= Qn::StreamQualityCount)
        value -= Qn::StreamQualityCount;
    return static_cast<Qn::StreamQuality>(value);
}

bool CameraScheduleWidget::isCurrentBitrateCustom() const
{
    return ui->qualityComboBox->currentData().toInt() >= Qn::StreamQualityCount;
}

void CameraScheduleWidget::setAdvancedSettingsVisible(bool value)
{
    const auto text = value ? tr("Less Settings") : tr("More Settings");
    const auto icon = qnSkin->icon(value ? lit("buttons/collapse.png") : lit("buttons/expand.png"));
    ui->advancedSettingsButton->setText(text);
    ui->advancedSettingsButton->setIcon(icon);
    ui->advancedSettingsWidget->setVisible(value);
    ui->settingsGroupBox->layout()->activate();
    m_advancedSettingsVisible = value;
}

QPair<Qn::StreamQuality, bool> CameraScheduleWidget::qualityForBitrate(qreal bitrateMbps) const
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

qreal CameraScheduleWidget::bitrateForQuality(Qn::StreamQuality quality) const
{
    if (!m_advancedSettingsSupported)
        return 0.0;

    if (m_cameras.size() != 1)
    {
        NX_ASSERT(false, Q_FUNC_INFO);
        return 0.0;
    }

    return getBitrateForQuality(m_cameras.front(), quality,
        ui->fpsSpinBox->value(), ui->bitrateSpinBox->decimals());
}

void CameraScheduleWidget::updateMotionAvailable()
{
    using boost::algorithm::all_of;

    bool available = all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return !camera->isDtsBased(); });
    if (m_motionTypeOverride != Qn::MT_Default)
    {
        available &= m_motionTypeOverride != Qn::MT_NoMotion;
    }
    else
    {
        available &= all_of(m_cameras, [](const QnVirtualCameraResourcePtr &camera) { return camera->hasMotion(); });
    }

    if (m_motionAvailable == available)
        return;

    m_motionAvailable = available;

    updateMotionButtons();
}

void CameraScheduleWidget::setExportScheduleButtonEnabled(bool enabled)
{
    ui->exportScheduleButton->setEnabled(enabled);
    ui->exportWarningLabel->setVisible(!enabled);
}

void CameraScheduleWidget::updatePanicLabelText()
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

QnScheduleTaskList CameraScheduleWidget::scheduleTasks() const
{
    QnScheduleTaskList tasks;
    if (!ui->gridWidget->isActive())
        return tasks;

    for (int row = 0; row < ui->gridWidget->rowCount(); ++row)
    {
        QnScheduleTask::Data task;

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

            if (task.m_startTime == task.m_endTime)
            {
                // An invalid one; initialize.
                task.m_dayOfWeek = row + 1;
                task.m_startTime = col * 3600; //< In secs from start of day.
                task.m_endTime = (col + 1) * 3600; //< In secs from start of day.
                task.m_recordType = recordType;
                task.m_streamQuality = streamQuality;
                task.m_bitrateKbps = bitrateKbps;
                task.m_fps = fps;
                ++col;
            }
            else if (task.m_recordType == recordType
                && task.m_streamQuality == streamQuality
                && task.m_bitrateKbps == bitrateKbps
                && task.m_fps == fps)
            {
                task.m_endTime = (col + 1) * 3600; //< In secs from start of day.
                ++col;
            }
            else
            {
                tasks.append(task);
                task = QnScheduleTask::Data();
            }
        }

        if (task.m_startTime != task.m_endTime)
            tasks.append(task);
    }

    return tasks;
}

void CameraScheduleWidget::setScheduleTasks(const QnScheduleTaskList& value)
{
    QSignalBlocker gridBlocker(ui->gridWidget);

    QnScheduleTaskList tasks = value;

    ui->gridWidget->resetCellValues();
    if (tasks.isEmpty())
    {
        for (int nDay = 1; nDay <= 7; ++nDay)
        {
            QnScheduleTask::Data data;
            data.m_dayOfWeek = nDay;
            data.m_startTime = 0;
            data.m_endTime = 86400;
            data.m_beforeThreshold = kDefaultBeforeThresholdSec;
            data.m_afterThreshold = kDefaultAfterThresholdSec;
            tasks << data;
        }
    }

    auto task = tasks.first();
    ui->recordBeforeSpinBox->setValue(task.getBeforeThreshold());
    ui->recordAfterSpinBox->setValue(task.getAfterThreshold());

    for (const auto &task : tasks)
    {
        const int row = task.getDayOfWeek() - 1;
        Qn::StreamQuality quality = Qn::QualityNotDefined;
        qreal bitrateMbps = 0.0;

        if (task.getRecordingType() != Qn::RT_Never)
        {
            switch (task.getStreamQuality())
            {
                case Qn::QualityLow:
                case Qn::QualityNormal:
                case Qn::QualityHigh:
                case Qn::QualityHighest:
                    quality = task.getStreamQuality();
                    bitrateMbps = task.getBitrateKbps() / kKbpsInMbps;
                    break;
                default:
                    qWarning("CameraScheduleWidget::setScheduleTasks(): Unhandled StreamQuality value %d.", task.getStreamQuality());
                    break;
            }
        }

        for (int col = task.getStartTime() / 3600; col < task.getEndTime() / 3600; ++col)
        {
            const QPoint cell(col, row);
            QnScheduleGridWidget::CellParams params;
            params.recordingType = task.getRecordingType();
            params.quality = quality;
            params.fps = task.getFps();
            params.bitrateMbps = m_advancedSettingsSupported ? bitrateMbps : 0.0;
            ui->gridWidget->setCellValue(cell, params);
        }
    }

    if (!m_updating)
        emit scheduleTasksChanged();
}

bool CameraScheduleWidget::canEnableRecording() const
{
    QnCamLicenseUsageHelper licenseHelper(m_cameras, true, commonModule());
    return all_of(m_cameras,
        [&licenseHelper](const QnVirtualCameraResourcePtr& camera)
        {
            return licenseHelper.isValid(camera->licenseType());
        });
}

void CameraScheduleWidget::updateRecordThresholds(QnScheduleTaskList& tasks)
{
    int before = ui->recordBeforeSpinBox->value();
    int after = ui->recordAfterSpinBox->value();
    for (auto& task : tasks)
    {
        task.setBeforeThreshold(before);
        task.setAfterThreshold(after);
    }
}

void CameraScheduleWidget::updateScheduleTypeControls()
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

void CameraScheduleWidget::updateGridParams(bool pickedFromGrid)
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
        qWarning() << "CameraScheduleWidget::No record type is selected!";

    updateScheduleTypeControls();
    bool enabled = !ui->noRecordButton->isChecked();
    ui->fpsSpinBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->qualityComboBox->setEnabled(enabled && m_recordingParamsAvailable);
    ui->advancedSettingsWidget->setEnabled(enabled && m_recordingParamsAvailable);
    updateRecordSpinboxes();

    if (!(m_readOnly && pickedFromGrid))
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
    emit gridParamsChanged();
}

void CameraScheduleWidget::setFps(int value)
{
    ui->fpsSpinBox->setValue(value);
}

int CameraScheduleWidget::getGridMaxFps(bool motionPlusLqOnly)
{
    return ui->gridWidget->getMaxFps(motionPlusLqOnly);
}

void CameraScheduleWidget::setScheduleEnabled(bool enabled)
{
    ui->enableRecordingCheckBox->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
}

bool CameraScheduleWidget::isScheduleEnabled() const
{
    return ui->enableRecordingCheckBox->checkState() != Qt::Unchecked;
}

void CameraScheduleWidget::updateArchiveRangeEnabledState()
{
    ui->spinBoxMaxDays->setEnabled(ui->checkBoxMaxArchive->checkState() == Qt::Unchecked);
    ui->spinBoxMinDays->setEnabled(ui->checkBoxMinArchive->checkState() == Qt::Unchecked);
    validateArchiveLength();
}

void CameraScheduleWidget::updateGridEnabledState()
{
    const bool recordingEnabled = ui->enableRecordingCheckBox->isChecked();
    ui->motionGroupBox->setEnabled(m_recordingParamsAvailable);
    setLayoutEnabled(ui->recordingScheduleLayout, recordingEnabled);
    setLayoutEnabled(ui->scheduleSettingsLayout, recordingEnabled);
    setLayoutEnabled(ui->bottomParametersLayout, recordingEnabled);
    updateScheduleTypeControls();
    updateArchiveRangeEnabledState();
}

void CameraScheduleWidget::updateLicensesLabelText()
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

void CameraScheduleWidget::updateLicensesButtonVisible()
{
    ui->licensesButton->setVisible(accessController()->hasGlobalPermission(Qn::GlobalAdminPermission));
}

void CameraScheduleWidget::updateRecordSpinboxes()
{
    ui->recordBeforeSpinBox->setEnabled(m_motionAvailable);
    ui->recordAfterSpinBox->setEnabled(m_motionAvailable);
}

void CameraScheduleWidget::cameraResourceChanged()
{
    updateMaxFPS();
    updateMotionButtons();
}

void CameraScheduleWidget::updateMotionButtons()
{
    bool hasDualStreaming = !m_cameras.isEmpty();
    bool hasMotion = !m_cameras.isEmpty();
    for (const auto &camera: m_cameras)
    {
        hasDualStreaming &= camera->hasDualStreaming2();
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

void CameraScheduleWidget::updateMaxFpsValue(bool motionPlusLqToggled)
{
    int maximum = motionPlusLqToggled ? m_maxDualStreamingFps : m_maxFps;

    /* This check is necessary because Qt doesn't do it and resets spinbox state: */
    if (maximum == ui->fpsSpinBox->maximum())
        return;

    ui->fpsSpinBox->setMaximum(maximum);
}

void CameraScheduleWidget::updateRecordingParamsAvailable()
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

void CameraScheduleWidget::updateColors()
{
    ui->recordAlwaysButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_Always));
    ui->recordMotionButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_MotionOnly));
    ui->recordMotionPlusLQButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_MotionAndLowQuality));
    ui->noRecordButton->setCustomPaintFunction(ui->gridWidget->paintFunction(Qn::RT_Never));
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void CameraScheduleWidget::at_gridWidget_cellActivated(const QPoint &cell)
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

void CameraScheduleWidget::at_displayQualiteCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowQuality(state && m_recordingParamsAvailable);
}

void CameraScheduleWidget::at_displayFpsCheckBox_stateChanged(int state)
{
    ui->gridWidget->setShowFps(state && m_recordingParamsAvailable);
}

void CameraScheduleWidget::at_licensesButton_clicked()
{
    menu()->trigger(action::PreferencesLicensesTabAction);
}

void CameraScheduleWidget::at_releaseSignalizer_activated(QObject *target)
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
        else /* Has dual streaming but not motion */
        {
            QnMessageBox::warning(this,
                tr("Motion detection disabled"),
                tr("To enable or adjust it, go to the \"Motion\" tab in Camera Settings."));
        }
    }
}

void CameraScheduleWidget::at_exportScheduleButton_clicked()
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
            {
                int maxDays = maxRecordedDays();
                if (maxDays != kRecordedDaysDontChange)
                    camera->setMaxDays(maxDays);
                int minDays = minRecordedDays();
                if (minDays != kRecordedDaysDontChange)
                    camera->setMinDays(minDays);
            }

            camera->setScheduleDisabled(!recordingEnabled);
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
                if (task.getRecordingType() == Qn::RT_MotionAndLowQuality)
                    task.setFps(qMin(task.getFps(), maxFps - decreaseIfMotionPlusLQ));
                else
                    task.setFps(qMin(task.getFps(), maxFps - decreaseAlways));

                if (const auto bitrate = task.getBitrateKbps()) // Try to calculate new custom bitrate
                {
                    if (!camera->cameraMediaCapability().isNull())
                    {
                        // Target camera supports custom bitrate
                        const auto normalBitrate = getBitrateForQuality(sourceCamera,
                            task.getStreamQuality(), task.getFps(), ui->bitrateSpinBox->decimals());
                        const auto bitrateAspect = (bitrate - normalBitrate) / normalBitrate;

                        const auto targetNormalBitrate = getBitrateForQuality(camera,
                            task.getStreamQuality(), task.getFps(), ui->bitrateSpinBox->decimals());
                        const auto targetBitrate = targetNormalBitrate * bitrateAspect;
                        task.setBitrateKbps(targetBitrate);
                    }
                    else
                        task.setBitrateKbps(0);
                }
                tasks.append(task);
            }
            updateRecordThresholds(tasks);

            camera->setScheduleTasks(tasks);
        };

    auto selectedCameras = resourcePool()->getResources<QnVirtualCameraResource>(
        dialog->selectedResources());
    qnResourcesChangesManager->saveCameras(selectedCameras, applyChanges);
    updateLicensesLabelText();
}

bool CameraScheduleWidget::hasMotionOnGrid() const
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

bool CameraScheduleWidget::hasDualStreamingMotionOnGrid() const
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

int CameraScheduleWidget::maxRecordedDays() const
{
    switch (ui->checkBoxMaxArchive->checkState())
    {
        case Qt::Unchecked:
            return ui->spinBoxMaxDays->value();
        case Qt::Checked:   //automatically manage but save for future use
            return ui->spinBoxMaxDays->value() * -1;
        default:
            return kRecordedDaysDontChange;
    }
}

int CameraScheduleWidget::minRecordedDays() const
{
    switch (ui->checkBoxMinArchive->checkState())
    {
        case Qt::Unchecked:
            return ui->spinBoxMinDays->value();
        case Qt::Checked:   //automatically manage but save for future use
            return ui->spinBoxMinDays->value() * -1;
        default:
            return kRecordedDaysDontChange;
    }
}

void CameraScheduleWidget::validateArchiveLength()
{
    if (ui->checkBoxMinArchive->checkState() == Qt::Unchecked && ui->checkBoxMaxArchive->checkState() == Qt::Unchecked)
    {
        if (ui->spinBoxMaxDays->value() < ui->spinBoxMinDays->value())
            ui->spinBoxMaxDays->setValue(ui->spinBoxMinDays->value());
    }

    QString alertText;
    bool alertVisible = ui->spinBoxMinDays->isEnabled() && ui->spinBoxMinDays->value() > kDangerousMinArchiveDays;

    if (alertVisible)
    {
        alertText = QnDeviceDependentStrings::getDefaultNameFromSet(
            resourcePool(),
            tr("High minimum value can lead to archive length decrease on other devices."),
            tr("High minimum value can lead to archive length decrease on other cameras."));
    }

    setArchiveLengthAlert(alertText);
}

void CameraScheduleWidget::setScheduleAlert(const QString& scheduleAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_scheduleAlert = scheduleAlert;
    emit alert(m_scheduleAlert.isEmpty() ? m_archiveLengthAlert : m_scheduleAlert);
}

void CameraScheduleWidget::setArchiveLengthAlert(const QString& archiveLengthAlert)
{
    /* We want to force update - emit a signal - even if the text didn't change: */
    m_archiveLengthAlert = archiveLengthAlert;
    emit alert(m_archiveLengthAlert.isEmpty() ? m_scheduleAlert : m_archiveLengthAlert);
}

void CameraScheduleWidget::updateAlert(AlertReason when)
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

bool CameraScheduleWidget::isRecordingScheduled() const
{
    return any_of(scheduleTasks(),
        [](const QnScheduleTask& task) -> bool
        {
            return !task.isEmpty() && task.getRecordingType() != Qn::RT_Never;
        });
}

} // namespace desktop
} // namespace client
} // namespace nx
