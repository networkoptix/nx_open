#include "legacy_workbench_export_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>

#include <nx/utils/std/cpp14.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>
#include <client/client_module.h>

#include <common/common_globals.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <camera/client_video_camera.h>
#include <camera/resource_display.h>
#include <camera/camera_data_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/fusion/model_functions.h>

#include <core/resource/avi/avi_resource.h>
#include <core/storage/file_storage/layout_storage_resource.h>

#include <platform/environment.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <nx/client/desktop/legacy/legacy_export_settings.h>
#include <nx/client/desktop/legacy/legacy_export_media_tool.h>
#include <nx/client/desktop/legacy/legacy_export_layout_tool.h>

#include <nx/core/transcoding/filters/legacy_transcoding_settings.h>

#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/client/desktop/ui/dialogs/rapid_review_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/event_processors.h>

#include <nx/utils/app_info.h>
#include <nx/utils/string.h>

#include <utils/common/app_info.h>

namespace {

/** Maximum sane duration: 30 minutes of recording. */
static const qint64 maxRecordingDurationMsec = 1000 * 60 * 30;

static const QString filterSeparator(QLatin1String(";;"));

/** Default bitrate for exported file size estimate in megabytes per second. */
static const qreal kDefaultBitrateMBps = 0.5;

/** Exe files greater than 4 Gb are not working. */
static const qint64 kMaximimumExeFileSizeMb = 4096;

/** Reserving 200 Mb for client binaries. */
static const qint64 kReservedClientSizeMb = 200;

static const int kMsPerSecond = 1000;

static const int kBitsPerByte = 8;


/** Calculate estimated video size in megabytes. */
qint64 estimatedExportVideoSizeMb(const QnMediaResourcePtr& mediaResource, qint64 lengthMs)
{
    qreal maxBitrateMBps = kDefaultBitrateMBps;

    if (QnVirtualCameraResourcePtr camera = mediaResource.dynamicCast<QnVirtualCameraResource>())
    {
        auto bitrateInfos = QJson::deserialized<CameraBitrates>(
            camera->getProperty(Qn::CAMERA_BITRATE_INFO_LIST_PARAM_NAME).toUtf8());
        if (!bitrateInfos.streams.empty())
            maxBitrateMBps = std::max_element(
                bitrateInfos.streams.cbegin(), bitrateInfos.streams.cend(),
                [](const CameraBitrateInfo& l, const CameraBitrateInfo &r)
        {
            return l.actualBitrate < r.actualBitrate;
        })->actualBitrate;
    }

    return maxBitrateMBps * lengthMs / (kMsPerSecond * kBitsPerByte);
}

}

// -------------------------------------------------------------------------- //
// QnTimestampsCheckboxControlDelegate
// -------------------------------------------------------------------------- //
class QnTimestampsCheckboxControlDelegate: public QnAbstractWidgetControlDelegate
{
public:
    QnTimestampsCheckboxControlDelegate(const QString &target, QObject *parent = NULL):
        QnAbstractWidgetControlDelegate(parent),
        m_target(target)
    {
    }

    ~QnTimestampsCheckboxControlDelegate() {}

    void updateWidget(const QString &value) override
    {
        foreach(QWidget* widget, widgets())
            widget->setEnabled(value != m_target);
    }
private:
    QString m_target;
};

namespace nx {
namespace client {
namespace desktop {
namespace legacy {

// -------------------------------------------------------------------------- //
// WorkbenchExportHandler
// -------------------------------------------------------------------------- //
WorkbenchExportHandler::WorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(ui::action::ExportTimeSelectionAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_exportTimeSelectionAction_triggered);
    connect(action(ui::action::ExportLayoutAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_exportLayoutAction_triggered);
    connect(action(ui::action::ExportRapidReviewAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_exportRapidReviewAction_triggered);
    connect(action(ui::action::SaveLocalLayoutAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_saveLocalLayoutAction_triggered);
    connect(action(ui::action::SaveLocalLayoutAsAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_saveLocalLayoutAsAction_triggered);
}

QString WorkbenchExportHandler::binaryFilterName() const
{
#ifdef Q_OS_WIN
#ifdef Q_OS_WIN64
    return tr("Executable %1 Media File (x64) (*.exe)").arg(QnAppInfo::organizationName());
#else
    return tr("Executable %1 Media File (x86) (*.exe)").arg(QnAppInfo::organizationName());
#endif
#endif
    return QString();
}

bool WorkbenchExportHandler::lockFile(const QString &filename)
{
    if (m_filesIsUse.contains(filename))
    {
        QnMessageBox::warning(mainWindow(),
            tr("File already used for recording"),
            QFileInfo(filename).completeBaseName()
                + L'\n' + tr("Please choose another name or wait until recording is finished."));
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename))
    {

        QnFileMessages::overwriteFailed(mainWindow(),
            QFileInfo(filename).completeBaseName());
        return false;
    }

    m_filesIsUse << filename;
    return true;
}

void WorkbenchExportHandler::unlockFile(const QString &filename)
{
    m_filesIsUse.remove(filename);
}

bool WorkbenchExportHandler::isBinaryExportSupported() const
{
    if (nx::utils::AppInfo::isWindows())
        return !qnRuntime->isActiveXMode();
    return false;
}

bool WorkbenchExportHandler::saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
    const QnTimePeriod &exportPeriod,
    const QString &layoutFileName,
    ExportLayoutSettings::Mode mode,
    bool readOnly,
    bool cancellable)
{
    if (!validateItemTypes(layout))
        return false;

    QnProgressDialog* exportProgressDialog = new QnProgressDialog(mainWindow(), Qt::Dialog);

    if (!cancellable)
    {
        exportProgressDialog->setCancelButton(NULL);

        QnMultiEventEater *eventEater = new QnMultiEventEater(Qn::IgnoreEvent, exportProgressDialog);
        eventEater->addEventType(QEvent::KeyPress);
        eventEater->addEventType(QEvent::KeyRelease); /* So that ESC doesn't close the dialog. */
        eventEater->addEventType(QEvent::Close);
        exportProgressDialog->installEventFilter(eventEater);
    }

    exportProgressDialog->setWindowTitle(tr("Exporting Layout"));
    exportProgressDialog->setModal(false);
    exportProgressDialog->show();

    const auto filename = Filename::parse(layoutFileName);
    auto tool = new ExportLayoutTool({layout, exportPeriod, filename, mode, readOnly}, this);
    connect(tool, &ExportLayoutTool::rangeChanged, exportProgressDialog,
        &QnProgressDialog::setRange);
    connect(tool, &ExportLayoutTool::valueChanged, exportProgressDialog,
        &QnProgressDialog::setValue);
    connect(tool, &ExportLayoutTool::stageChanged, exportProgressDialog,
        &QnProgressDialog::setLabelText);

    connect(tool, &ExportLayoutTool::finished, exportProgressDialog, &QWidget::hide);
    connect(tool, &ExportLayoutTool::finished, exportProgressDialog, &QObject::deleteLater);
    connect(tool, &ExportLayoutTool::finished, this, &WorkbenchExportHandler::at_layout_exportFinished);
    connect(tool, &ExportLayoutTool::finished, tool, &QObject::deleteLater);
    connect(exportProgressDialog, &QnProgressDialog::canceled, tool, &ExportLayoutTool::stop);

    return tool->start();
}

QnMediaResourceWidget* WorkbenchExportHandler::extractMediaWidget(
    const ui::action::Parameters& parameters)
{
    if (parameters.size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(parameters.widget());

    if ((parameters.size() == 0) && display()->widgets().size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(display()->widgets().front());

    return dynamic_cast<QnMediaResourceWidget *>(display()->activeWidget());
}

void WorkbenchExportHandler::at_exportTimeSelectionAction_triggered()
{
    exportTimeSelection(menu()->currentParameters(sender()));
}


// TODO: #GDM Monstrous function, refactor required
// TODO: #ynikitenkov refactor to use QnResourcePtr
void WorkbenchExportHandler::exportTimeSelectionInternal(
    const QnMediaResourcePtr &mediaResource,
    const QnAbstractStreamDataProvider *dataProvider,
    const QnLayoutItemData &itemData,
    const QnTimePeriod &period,
    qint64 timelapseFrameStepMs
)
{
    qint64 durationMs = period.durationMs;
    auto loader = qnClientModule->cameraDataManager()->loader(mediaResource);
    if (loader)
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
        if (!periods.isEmpty())
            durationMs = periods.duration();
        NX_ASSERT(durationMs > 0, Q_FUNC_INFO, "Intersected periods must not be empty or infinite");
    }


    bool wasLoggedIn = !context()->user().isNull();
    static const qint64 kTimelapseBaseFrameStepMs = 1000 / ui::dialogs::ExportRapidReview::kResultFps;
    qint64 exportSpeed = qMax(1ll, timelapseFrameStepMs / kTimelapseBaseFrameStepMs);
    // TODO: #Elric implement more precise estimation
    if (durationMs / exportSpeed > maxRecordingDurationMsec && timelapseFrameStepMs == 0)
    {
        const auto confirmed = confirmExport(QnMessageBoxIcon::Warning,
            tr("You are about to export a long video"),
            tr("It may require over a gigabyte of HDD space and take several minutes to complete.")
                + L'\n' + tr("Export anyway?"));

        if (!confirmed)
            return;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QStringList filters{
        lit("Matroska (*.mkv)"),
        lit("MP4 (*.mp4)"),
        lit("AVI (*.avi)")
    };

    bool canUseBinaryExport = isBinaryExportSupported() && timelapseFrameStepMs == 0;
    if (canUseBinaryExport)
        filters << binaryFilterName();

    QString allowedFormatFilter = filters.join(filterSeparator);

    QString fileName;
    bool binaryExport = false;
    ImageCorrectionParams contrastParams = itemData.contrastParams;
    QnItemDewarpingParams dewarpingParams = itemData.dewarpingParams;
    int rotation = itemData.rotation;
    QRectF zoomRect = itemData.zoomRect;
    qreal customAr = mediaResource->customAspectRatio();

    QnTimeStampParams timestampParams;
    timestampParams.displayOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->displayOffset(mediaResource);

    QString namePart = nx::utils::replaceNonFileNameCharacters(mediaResource->toResourcePtr()->getName(), L'_');
    QString timePart = (mediaResource->toResource()->flags() & Qn::utc)
        ? QDateTime::fromMSecsSinceEpoch(period.startTimeMs + timestampParams.displayOffset).toString(lit("yyyy_MMM_dd_hh_mm_ss"))
        : QTime(0, 0, 0, 0).addMSecs(period.startTimeMs).toString(lit("hh_mm_ss"));
    QString suggestion = QnEnvironment::getUniqueFileName(previousDir, namePart + lit("_") + timePart);



    bool transcodeWarnShown = false;
    QnLegacyTranscodingSettings imageParameters;

    while (true)
    {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        QScopedPointer<QnSessionAwareFileDialog> dialog(new QnSessionAwareFileDialog(
            mainWindow(),
            tr("Export Video As..."),
            suggestion,
            allowedFormatFilter
            ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        setHelpTopic(dialog.data(), Qn::Exporting_Help);

        QnAbstractWidgetControlDelegate* delegate = canUseBinaryExport
            ? new QnTimestampsCheckboxControlDelegate(binaryFilterName(), this)
            : nullptr;

        QComboBox* comboBox = 0;
        bool transcodeCheckbox = false;
        if (mediaResource->hasVideo(dataProvider))
        {
            comboBox = new QComboBox(dialog.data());
            comboBox->addItem(tr("No Timestamp"), -1);
            comboBox->addItem(tr("Top Left Corner (requires transcoding)"), Qt::TopLeftCorner);
            comboBox->addItem(tr("Top Right Corner (requires transcoding)"), Qt::TopRightCorner);
            comboBox->addItem(tr("Bottom Left Corner (requires transcoding)"), Qt::BottomLeftCorner);
            comboBox->addItem(tr("Bottom Right Corner (requires transcoding)"), Qt::BottomRightCorner);

            bool isPanoramic = mediaResource->getVideoLayout(0)->channelCount() > 1;
            if (isPanoramic)
                comboBox->setCurrentIndex(comboBox->count() - 1); /* Bottom right, as on layout */

            dialog->addWidget(tr("Timestamps:"), comboBox, delegate);

            transcodeCheckbox = contrastParams.enabled || dewarpingParams.enabled || itemData.rotation || customAr || !zoomRect.isNull();
            if (transcodeCheckbox)
            {
                dialog->addCheckBox(tr("Apply filters: Rotation, Dewarping, Image Enhancement, Custom Aspect Ratio (requires transcoding)"), &transcodeCheckbox, delegate);
            }
        }

        if (!dialog->exec())
            return;

        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return;

        QString selectedExtension = dialog->selectedExtension();
        binaryExport = canUseBinaryExport
            ? selectedExtension.contains(lit(".exe"))
            : false;

        if (comboBox)
        {
            int corner = comboBox->itemData(comboBox->currentIndex()).toInt();
            timestampParams.enabled = (corner >= 0);
            if (timestampParams.enabled)
                timestampParams.corner = static_cast<Qt::Corner>(corner);
        }

        if (binaryExport)
        {
            if (exeFileIsTooBig(mediaResource, period)
                && !confirmExportTooBigExeFile())
                    continue;

            transcodeCheckbox = false;
            timestampParams.enabled = false;
        }

        if (!transcodeCheckbox)
        {
            contrastParams.enabled = false;
            dewarpingParams.enabled = false;
            rotation = 0;
            zoomRect = QRectF();
            customAr = 0.0;
        }

        if (selectedExtension.contains(lit(".avi")))
        {
            if (auto loader = qnClientModule->cameraDataManager()->loader(mediaResource))
            {
                QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
                if (periods.size() > 1)
                {
                    const auto confirmed = confirmExport(QnMessageBoxIcon::Warning,
                        tr("AVI format is not recommended"),
                        tr("For exporting a non-continuous recording"
                            " MKV or some other format is recommended.")
                            + L'\n' + tr("Export to AVI anyway?"));

                    if (!confirmed)
                        continue;
                }
            }
        }

        imageParameters.zoomWindow = zoomRect;
        imageParameters.contrastParams = contrastParams;
        imageParameters.itemDewarpingParams = dewarpingParams;
        imageParameters.resource = mediaResource;
        imageParameters.rotation = rotation;
        imageParameters.forcedAspectRatio = customAr;
        imageParameters.timestampParams = timestampParams;

        auto videoLayout = mediaResource->getVideoLayout();
        bool doTranscode = transcodeCheckbox ||
            timestampParams.enabled ||
            (!binaryExport && videoLayout && videoLayout->channelCount() > 1);

        if (doTranscode)
        {
            const QnVirtualCameraResourcePtr camera = mediaResource.dynamicCast<QnVirtualCameraResource>();
            if (camera && !transcodeWarnShown)
            {
                const auto stream = camera->defaultStream();
                const auto resolution = stream.getResolution();

                if (!resolution.isEmpty())
                {
                    const int bigValue = std::numeric_limits<int>::max();
                    NX_ASSERT(resolution.isValid());

                    auto filterChain = QnImageFilterHelper::createFilterChain(imageParameters);
                    filterChain.prepare(imageParameters.resource, resolution,
                        QSize(bigValue, bigValue));
                    if (filterChain.isDownscaleRequired(resolution))
                    {
                        transcodeWarnShown = true;
                        const auto confirmed = confirmExport(QnMessageBoxIcon::Warning,
                            tr("Selected format not recommended"),
                            tr("To avoid video downscaling,"
                                " NOV or EXE formats are recommended for this camera.")
                                + L'\n' + tr("Export anyway?"));

                        if (!confirmed)
                        {
                            transcodeWarnShown = false;
                            continue;
                        }
                    }

                }
            }

            if (!transcodeWarnShown)
            {
                transcodeWarnShown = true;
                const auto confirmed = confirmExport(QnMessageBoxIcon::Question,
                    tr("Export with transcoding?"),
                    tr("It will increase CPU usage and may take significant time."));

                if (!confirmed)
                {
                    transcodeWarnShown = false;
                    continue;
                }
            }
        }


        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;


        if (!fileName.toLower().endsWith(selectedExtension))
        {
            fileName += selectedExtension;

            // method called under condition because in other case this message is popped out by the dialog itself
            if (QFile::exists(fileName))
            {
                if (!QnFileMessages::confirmOverwrite(
                    mainWindow(), QFileInfo(fileName).completeBaseName()))
                {
                    continue;
                }
            }
        }

        if (!lockFile(fileName))
        {
            continue;
        }

        break;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());


    if (binaryExport)
    {
        QnLayoutResourcePtr newLayout(new QnLayoutResource());

        NX_ASSERT(!itemData.uuid.isNull(), Q_FUNC_INFO, "Make sure itemData is valid");
        newLayout->addItem(itemData);
        saveLayoutToLocalFile(newLayout,
            period,
            fileName,
            ExportLayoutSettings::Mode::Export,
            false,
            true);
    }
    else
    {
        QnProgressDialog *exportProgressDialog = new QnSessionAware<QnProgressDialog>(mainWindow());
        exportProgressDialog->setWindowTitle(tr("Exporting Video"));
        exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
        exportProgressDialog->setModal(false);

        qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaResource, Qn::InvalidUtcOffset);

        LegacyExportMediaSettings settings;
        settings.mediaResource = mediaResource;
        settings.timePeriod = period;
        settings.fileName = fileName;
        settings.imageParameters = imageParameters;
        settings.serverTimeZoneMs = serverTimeZone;
        settings.timelapseFrameStepMs = timelapseFrameStepMs;

        auto tool = new LegacyExportMediaTool(settings, this);

        connect(exportProgressDialog, &QnProgressDialog::canceled, tool, &LegacyExportMediaTool::stop);

        connect(tool, &LegacyExportMediaTool::finished, exportProgressDialog, &QWidget::hide);
        connect(tool, &LegacyExportMediaTool::finished, exportProgressDialog, &QnProgressDialog::deleteLater);
        connect(tool, &LegacyExportMediaTool::finished, this, &WorkbenchExportHandler::at_camera_exportFinished);
        connect(tool, &LegacyExportMediaTool::rangeChanged, exportProgressDialog, &QnProgressDialog::setRange);
        connect(tool, &LegacyExportMediaTool::valueChanged, exportProgressDialog, &QnProgressDialog::setValue);

        tool->start();
        exportProgressDialog->show();
    }
}

bool WorkbenchExportHandler::exeFileIsTooBig(
    const QnMediaResourcePtr& mediaResource,
    const QnTimePeriod& period) const
{
    qint64 videoSizeMb = estimatedExportVideoSizeMb(mediaResource, period.durationMs);
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool WorkbenchExportHandler::exeFileIsTooBig(
    const QnLayoutResourcePtr& layout,
    const QnTimePeriod& period) const
{
    qint64 videoSizeMb = 0;
    for (const auto& resource: layout->layoutResources())
    {
        if (const QnMediaResourcePtr& media = resource.dynamicCast<QnMediaResource>())
            videoSizeMb += estimatedExportVideoSizeMb(media, period.durationMs);
    }
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool WorkbenchExportHandler::confirmExportTooBigExeFile() const
{
    return confirmExport(QnMessageBoxIcon::Warning,
        tr("EXE format not recommended"),
        tr("EXE files over 4 GB cannot be opened by double click due to a Windows limitation.")
            + L'\n' + tr("Export to EXE anyway?"));
}

void WorkbenchExportHandler::exportTimeSelection(const ui::action::Parameters& parameters,
    qint64 timelapseFrameStepMs)
{
    const auto widget = extractMediaWidget(parameters);
    QnMediaResourcePtr mediaResource = parameters.resource().dynamicCast<QnMediaResource>();

    /* Either resource or widget must be provided */
    if (!mediaResource && !widget)
    {
        NX_ASSERT(false, "Exactly one item must be selected for export,"
            " but several items are currently selected");
        return;
    }

    if (!mediaResource)
        mediaResource = widget->resource();

    QnVirtualCameraResourcePtr camera = mediaResource.dynamicCast<QnVirtualCameraResource>();
    auto dataProvider = camera
        ? camera->createDataProvider(Qn::CR_Default)
        : widget
        ? widget->display()->dataProvider()
        : nullptr;

    if (!mediaResource || !dataProvider)
        return;

    // Creates default layout item data (if there is no widget
    // selected - bookmarks export, for example). Media resource
    // is used because it should be presented to export data
    const auto createDefaultLayoutItemData =
        [](const QnMediaResourcePtr &mediaResource) -> QnLayoutItemData
    {
        const auto resource = mediaResource->toResourcePtr();

        QnLayoutItemData result;
        result.uuid = QnUuid::createUuid();
        result.resource.uniqueId = resource->getUniqueId();
        result.resource.id = resource->getId();
        result.flags = (Qn::SingleSelectedRole | Qn::SingleRole);
        result.combinedGeometry = QRect(0, 0, 1, 1);
        result.rotation = resource->hasProperty(QnMediaResource::rotationKey())
            ? resource->getProperty(QnMediaResource::rotationKey()).toInt()
            : 0;
        return result;
    };

    QnLayoutItemData itemData = widget
        ? widget->item()->data()
        : createDefaultLayoutItemData(mediaResource);

    const auto bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);
    const auto period = bookmark.isNull()
        ? parameters.argument<QnTimePeriod>(Qn::TimePeriodRole)
        : QnTimePeriod(bookmark.startTimeMs, bookmark.durationMs);

    exportTimeSelectionInternal(mediaResource, dataProvider, itemData, period, timelapseFrameStepMs);
}

void WorkbenchExportHandler::at_layout_exportFinished(bool success, const QString &filename)
{
    unlockFile(filename);

    auto tool = dynamic_cast<ExportLayoutTool*>(sender());
    if (!tool)
        return;

    if (success)
    {
        if (tool->mode() == ExportLayoutSettings::Mode::Export)
            showExportCompleteMessage();
    }
    else if (!tool->errorMessage().isEmpty())
    {
        QnMessageBox::critical(mainWindow(),
            tr("Failed to export Multi-Video"), tool->errorMessage());
    }
}


bool WorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr &layout)
{
    bool hasImage = false;
    bool hasLocal = false;

    for (const QnLayoutItemData &item : layout->getItems())
    {
        QnResourcePtr resource = resourcePool()->getResourceByDescriptor(item.resource);
        if (!resource)
            continue;
        if (resource->getParentResource() == layout)
            continue;
        hasImage |= resource->hasFlags(Qn::still_image);
        hasLocal |= resource->hasFlags(Qn::local);
        if (hasImage || hasLocal)
            break;
    }

    const auto showWarning =
        [this]()
        {
            QnMessageBox::warning(mainWindow(),
                tr("Local files not allowed for Multi-Video export"),
                tr("Please remove all local files from the layout and try again.")
            );
        };

    if (hasImage)
    {
        showWarning();
        return false;
    }
    else if (hasLocal)
    {
        /* Always allow export selected area. */
        if (layout->getItems().size() == 1)
            return true;

        showWarning();
        return false;
    }
    return true;
}

bool WorkbenchExportHandler::saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable)
{
    return saveLayoutToLocalFile(layout,
        layout->getLocalRange(),
        layout->getUrl(),
        ExportLayoutSettings::Mode::LocalSave,
        readOnly,
        cancellable);
}

bool WorkbenchExportHandler::doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod,
    const QnLayoutResourcePtr &layout,
    ExportLayoutSettings::Mode mode)
{
    // TODO: #Elric we have a lot of copypasta with at_exportTimeSelectionAction_triggered

    bool wasLoggedIn = !context()->user().isNull();

    if (!validateItemTypes(layout))
        return false;

    QString dialogName;
    if (mode == ExportLayoutSettings::Mode::LocalSaveAs)
        dialogName = tr("Save local layout as...");
    else if (mode == ExportLayoutSettings::Mode::Export)
        dialogName = tr("Export Layout As...");
    else
        return false; // not used

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QString baseName = layout->getName();
    if (qnRuntime->isActiveXMode() || baseName.isEmpty())
        baseName = tr("exported");
    QString suggestion = nx::utils::replaceNonFileNameCharacters(baseName, QLatin1Char('_'));
    suggestion = QnEnvironment::getUniqueFileName(previousDir, QFileInfo(suggestion).baseName());

    QString fileName;
    bool readOnly = false;

    QString mediaFileFilter = tr("%1 Media File (*.nov)").arg(QnAppInfo::organizationName());
    if (isBinaryExportSupported())
        mediaFileFilter += filterSeparator + binaryFilterName();

    while (true)
    {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        QScopedPointer<QnSessionAwareFileDialog> dialog(new QnSessionAwareFileDialog(
            mainWindow(),
            dialogName,
            suggestion,
            mediaFileFilter
            ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        dialog->addCheckBox(tr("Make file read-only."), &readOnly);

        setHelpTopic(dialog.data(), Qn::Exporting_Layout_Help);

        if (!dialog->exec())
            return false;

        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return false;

        QString selectedExtension = dialog->selectedExtension();
        bool binaryExport = isBinaryExportSupported()
            ? selectedExtension.contains(lit(".exe"))
            : false;

        if (binaryExport
            && exeFileIsTooBig(layout, exportPeriod)
            && !confirmExportTooBigExeFile())
                continue;

        if (!fileName.toLower().endsWith(selectedExtension))
        {
            fileName += selectedExtension;

            // method called under condition because in other case this message is popped out by the dialog itself
            if (QFile::exists(fileName) &&
                !QnFileMessages::confirmOverwrite(
                    mainWindow(), QFileInfo(fileName).completeBaseName()))
            {
                return false;
            }
        }

        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        if (!lockFile(fileName))
            continue;

        break;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return false;

    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());

    saveLayoutToLocalFile(layout, exportPeriod, fileName, mode, readOnly, true);

    return true;
}

bool WorkbenchExportHandler::confirmExport(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras) const
{
    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());

    dialog.addButton(tr("Export"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    return (dialog.exec() != QDialogButtonBox::Cancel);
}

void WorkbenchExportHandler::at_exportLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    bool wasLoggedIn = !context()->user().isNull();

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    QnTimePeriod exportPeriod = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    if (exportPeriod.durationMs * layout->getItems().size() > 1000 * 60 * 30)
    {
        const auto confirmed = confirmExport(QnMessageBoxIcon::Warning,
            tr("You are about to export a lot of video"),
            tr("It may require over a gigabyte of HDD space and take several minutes to complete.")
                + L'\n' + tr("Export anyway?"));

        if (!confirmed)
            return;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    doAskNameAndExportLocalLayout(exportPeriod, layout, ExportLayoutSettings::Mode::Export);
}

void WorkbenchExportHandler::at_exportRapidReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    qint64 durationMs = period.durationMs;
    QnMediaResourcePtr mediaResource = parameters.resource().dynamicCast<QnMediaResource>();
    auto loader = qnClientModule->cameraDataManager()->loader(mediaResource);
    if (loader)
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
        if (!periods.isEmpty())
            durationMs = periods.duration();
        NX_ASSERT(durationMs > 0, Q_FUNC_INFO, "Intersected periods must not be empty or infinite");
    }

    if (durationMs < ui::dialogs::ExportRapidReview::kMinimalSourcePeriodLength)
    {
        QnMessageBox::warning(mainWindow(),
            tr("Too short period selected"),
            tr("For exporting as Rapid Review, video length should be at least 10 seconds."));
        return;
    }

    auto dialog = std::make_unique<ui::dialogs::ExportRapidReview>(mainWindow());
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setSourcePeriodLengthMs(durationMs);
    int speed = qnSettings->timelapseSpeed();
    if (speed > 0)
        dialog->setSpeed(speed);

    if (!dialog->exec())
        return;

    qnSettings->setTimelapseSpeed(dialog->speed());
    exportTimeSelection(parameters, dialog->frameStepMs());
}

void WorkbenchExportHandler::at_saveLocalLayoutAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto layout = parameters.resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout && layout->isFile());
    if (!layout || !layout->isFile())
        return;

    const bool readOnly = !accessController()->hasPermissions(layout,
        Qn::WritePermission);
    saveLocalLayout(layout, readOnly, true); //< Overwrite layout file.
}

void WorkbenchExportHandler::at_saveLocalLayoutAsAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto layout = parameters.resource().dynamicCast<QnLayoutResource>();
    NX_ASSERT(layout && layout->isFile());
    if (!layout || !layout->isFile())
        return;

    doAskNameAndExportLocalLayout(layout->getLocalRange(),
        layout,
        ExportLayoutSettings::Mode::LocalSaveAs);
}

void WorkbenchExportHandler::showExportCompleteMessage()
{
    QnMessageBox::success(mainWindow(), tr("Export completed"));
}

void WorkbenchExportHandler::at_camera_exportFinished(bool success, const QString &fileName)
{
    unlockFile(fileName);

    LegacyExportMediaTool *tool = qobject_cast<LegacyExportMediaTool*>(sender());
    if (!tool)
        return;

    tool->deleteLater();

    if (success)
    {
        QnAviResourcePtr file(new QnAviResource(fileName));
        file->setStatus(Qn::Online);
        resourcePool()->addResource(file);
        showExportCompleteMessage();
    }
    else if (tool->status() != StreamRecorderError::noError)
    {
        QnMessageBox::critical(mainWindow(),
            tr("Failed to export video"),
            QnStreamRecorder::errorString(tool->status()));
    }
}

} // namespace legacy
} // namespace desktop
} // namespace client
} // namespace nx
