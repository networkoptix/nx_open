#include "workbench_export_handler.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QComboBox>

#include <nx/utils/std/cpp14.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <common/common_globals.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <camera/client_video_camera.h>
#include <camera/client_video_camera_export_tool.h>
#include <camera/camera_data_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/fusion/model_functions.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <platform/environment.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <nx/client/desktop/ui/dialogs/rapid_review_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/extensions/workbench_layout_export_tool.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <ui/help/help_topics.h>
#include <ui/help/help_topic_accessor.h>

#include <utils/common/event_processors.h>

#include <nx/utils/string.h>

#include <utils/common/app_info.h>

using namespace nx::client::desktop::ui;

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


// -------------------------------------------------------------------------- //
// QnWorkbenchExportHandler
// -------------------------------------------------------------------------- //
QnWorkbenchExportHandler::QnWorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(action::ExportTimeSelectionAction), &QAction::triggered, this,
        &QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered);
    connect(action(action::ExportLayoutAction), &QAction::triggered, this,
        &QnWorkbenchExportHandler::at_exportLayoutAction_triggered);
    connect(action(action::ExportRapidReviewAction), &QAction::triggered, this,
        &QnWorkbenchExportHandler::at_exportRapidReviewAction_triggered);
}

QString QnWorkbenchExportHandler::binaryFilterName() const
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

bool QnWorkbenchExportHandler::lockFile(const QString &filename)
{
    if (m_filesIsUse.contains(filename))
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("File already used for recording"),
            QFileInfo(filename).completeBaseName()
                + L'\n' + tr("Please choose another name or wait until recording is finished."));
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename))
    {

        QnFileMessages::overwriteFailed(mainWindowWidget(),
            QFileInfo(filename).completeBaseName());
        return false;
    }

    m_filesIsUse << filename;
    return true;
}

void QnWorkbenchExportHandler::unlockFile(const QString &filename)
{
    m_filesIsUse.remove(filename);
}

bool QnWorkbenchExportHandler::isBinaryExportSupported() const
{
#ifdef Q_OS_WIN
    return !qnRuntime->isActiveXMode();
#else
    return false;
#endif
}


bool QnWorkbenchExportHandler::saveLayoutToLocalFile(const QnLayoutResourcePtr &layout,
    const QnTimePeriod &exportPeriod,
    const QString &layoutFileName,
    Qn::LayoutExportMode mode,
    bool readOnly,
    bool cancellable,
    QObject *target,
    const char *slot)
{
    if (!validateItemTypes(layout))
        return false;

    QnProgressDialog* exportProgressDialog = new QnProgressDialog(mainWindowWidget(), Qt::Dialog);

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

    QnLayoutExportTool* tool = new QnLayoutExportTool(layout, exportPeriod, layoutFileName, mode, readOnly, this);
    connect(tool, SIGNAL(rangeChanged(int, int)), exportProgressDialog, SLOT(setRange(int, int)));
    connect(tool, SIGNAL(valueChanged(int)), exportProgressDialog, SLOT(setValue(int)));
    connect(tool, SIGNAL(stageChanged(QString)), exportProgressDialog, SLOT(setLabelText(QString)));

    connect(tool, &QnLayoutExportTool::finished, exportProgressDialog, &QWidget::hide);
    connect(tool, &QnLayoutExportTool::finished, exportProgressDialog, &QObject::deleteLater);
    connect(tool, &QnLayoutExportTool::finished, this, &QnWorkbenchExportHandler::at_layout_exportFinished);
    connect(tool, &QnLayoutExportTool::finished, tool, &QObject::deleteLater);

    connect(exportProgressDialog, SIGNAL(canceled()), tool, SLOT(stop()));
    if (target && slot)
        connect(tool, SIGNAL(finished(bool, QString)), target, slot);

    return tool->start();
}

QnMediaResourceWidget* QnWorkbenchExportHandler::extractMediaWidget(const action::Parameters& parameters)
{
    if (parameters.size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(parameters.widget());

    if ((parameters.size() == 0) && display()->widgets().size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(display()->widgets().front());

    return dynamic_cast<QnMediaResourceWidget *>(display()->activeWidget());
}

void QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered()
{
    exportTimeSelection(menu()->currentParameters(sender()));
}


// TODO: #GDM Monstrous function, refactor required
// TODO: #ynikitenkov refactor to use QnResourcePtr
void QnWorkbenchExportHandler::exportTimeSelectionInternal(
    const QnMediaResourcePtr &mediaResource,
    const QnAbstractStreamDataProvider *dataProvider,
    const QnLayoutItemData &itemData,
    const QnTimePeriod &period,
    qint64 timelapseFrameStepMs
)
{
    qint64 durationMs = period.durationMs;
    auto loader = context()->instance<QnCameraDataManager>()->loader(mediaResource);
    if (loader)
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
        if (!periods.isEmpty())
            durationMs = periods.duration();
        Q_ASSERT_X(durationMs > 0, Q_FUNC_INFO, "Intersected periods must not be empty or infinite");
    }


    bool wasLoggedIn = !context()->user().isNull();
    static const qint64 kTimelapseBaseFrameStepMs = 1000 /  dialogs::ExportRapidReview::kResultFps;
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
    QnImageFilterHelper imageParameters;

    while (true)
    {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        QScopedPointer<QnSessionAwareFileDialog> dialog(new QnSessionAwareFileDialog(
            mainWindowWidget(),
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
            auto loader = context()->instance<QnCameraDataManager>()->loader(mediaResource);
            const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
            if (loader && archive)
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

        imageParameters.setSrcRect(zoomRect);
        imageParameters.setContrastParams(contrastParams);
        imageParameters.setDewarpingParams(mediaResource->getDewarpingParams(), dewarpingParams);
        imageParameters.setRotation(rotation);
        imageParameters.setCustomAR(customAr);
        imageParameters.setTimeStampParams(timestampParams);
        imageParameters.setVideoLayout(mediaResource->getVideoLayout());

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

                    auto filters = imageParameters.createFilterChain(resolution, QSize(bigValue, bigValue));
                    const QSize resultResolution = imageParameters.updatedResolution(filters, resolution);
                    if (resultResolution.width() > imageParameters.defaultResolutionLimit.width() ||
                        resultResolution.height() > imageParameters.defaultResolutionLimit.height())
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
                    mainWindowWidget(), QFileInfo(fileName).completeBaseName()))
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
        saveLayoutToLocalFile(newLayout, period, fileName, Qn::LayoutExport, false, true);
    }
    else
    {
        QnProgressDialog *exportProgressDialog = new QnSessionAware<QnProgressDialog>(mainWindowWidget());
        exportProgressDialog->setWindowTitle(tr("Exporting Video"));
        exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
        exportProgressDialog->setModal(false);

        qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(mediaResource, Qn::InvalidUtcOffset);

        QnClientVideoCameraExportTool *tool = new QnClientVideoCameraExportTool(
            mediaResource,
            period,
            fileName,
            imageParameters,
            serverTimeZone,
            timelapseFrameStepMs,
            this);

        connect(exportProgressDialog, &QnProgressDialog::canceled, tool, &QnClientVideoCameraExportTool::stop);

        connect(tool, &QnClientVideoCameraExportTool::finished, exportProgressDialog, &QWidget::hide);
        connect(tool, &QnClientVideoCameraExportTool::finished, exportProgressDialog, &QnProgressDialog::deleteLater);
        connect(tool, &QnClientVideoCameraExportTool::finished, this, &QnWorkbenchExportHandler::at_camera_exportFinished);
        connect(tool, &QnClientVideoCameraExportTool::rangeChanged, exportProgressDialog, &QnProgressDialog::setRange);
        connect(tool, &QnClientVideoCameraExportTool::valueChanged, exportProgressDialog, &QnProgressDialog::setValue);

        tool->start();
        exportProgressDialog->show();
    }
}

bool QnWorkbenchExportHandler::exeFileIsTooBig(
    const QnMediaResourcePtr& mediaResource,
    const QnTimePeriod& period) const
{
    qint64 videoSizeMb = estimatedExportVideoSizeMb(mediaResource, period.durationMs);
    return (videoSizeMb + kReservedClientSizeMb > kMaximimumExeFileSizeMb);
}

bool QnWorkbenchExportHandler::exeFileIsTooBig(
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

bool QnWorkbenchExportHandler::confirmExportTooBigExeFile() const
{
    return confirmExport(QnMessageBoxIcon::Warning,
        tr("EXE format not recommended"),
        tr("EXE files over 4 GB cannot be opened by double click due to a Windows limitation.")
            + L'\n' + tr("Export to EXE anyway?"));
}

void QnWorkbenchExportHandler::exportTimeSelection(const action::Parameters& parameters,
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

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    exportTimeSelectionInternal(mediaResource, dataProvider, itemData, period, timelapseFrameStepMs);
}

void QnWorkbenchExportHandler::at_layout_exportFinished(bool success, const QString &filename)
{
    unlockFile(filename);

    QnLayoutExportTool* tool = dynamic_cast<QnLayoutExportTool*>(sender());
    if (!tool)
        return;

    if (success)
    {
        if (tool->mode() == Qn::LayoutExport)
            showExportCompleteMessage();
    }
    else if (!tool->errorMessage().isEmpty())
    {
        QnMessageBox::critical(mainWindowWidget(),
            tr("Failed to export Multi-Video"), tool->errorMessage());
    }
}


bool QnWorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr &layout)
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
            QnMessageBox::warning(mainWindowWidget(),
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

bool QnWorkbenchExportHandler::saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable, QObject *target, const char *slot)
{
    return saveLayoutToLocalFile(layout, layout->getLocalRange(), layout->getUrl(), Qn::LayoutLocalSave, readOnly, cancellable, target, slot);
}

bool QnWorkbenchExportHandler::doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, const QnLayoutResourcePtr &layout, Qn::LayoutExportMode mode)
{
    // TODO: #Elric we have a lot of copypasta with at_exportTimeSelectionAction_triggered

    bool wasLoggedIn = !context()->user().isNull();

    if (!validateItemTypes(layout))
        return false;

    QString dialogName;
    if (mode == Qn::LayoutLocalSaveAs)
        dialogName = tr("Save local layout as...");
    else if (mode == Qn::LayoutExport)
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
            mainWindowWidget(),
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
                    mainWindowWidget(), QFileInfo(fileName).completeBaseName()))
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

bool QnWorkbenchExportHandler::confirmExport(
    QnMessageBoxIcon icon,
    const QString& text,
    const QString& extras) const
{
    QnMessageBox dialog(icon, text, extras,
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindowWidget());

    dialog.addButton(tr("Export"), QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Standard);
    return (dialog.exec() != QDialogButtonBox::Cancel);
}

void QnWorkbenchExportHandler::at_exportLayoutAction_triggered()
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

    doAskNameAndExportLocalLayout(exportPeriod, layout, Qn::LayoutExport);
}

void QnWorkbenchExportHandler::at_exportRapidReviewAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    qint64 durationMs = period.durationMs;
    QnMediaResourcePtr mediaResource = parameters.resource().dynamicCast<QnMediaResource>();
    auto loader = context()->instance<QnCameraDataManager>()->loader(mediaResource);
    if (loader)
    {
        QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
        if (!periods.isEmpty())
            durationMs = periods.duration();
        Q_ASSERT_X(durationMs > 0, Q_FUNC_INFO, "Intersected periods must not be empty or infinite");
    }

    if (durationMs < dialogs::ExportRapidReview::kMinimalSourcePeriodLength)
    {
        QnMessageBox::warning(mainWindowWidget(),
            tr("Too short period selected"),
            tr("For exporting as Rapid Review, video length should be at least 10 seconds."));
        return;
    }

    auto dialog = std::make_unique<dialogs::ExportRapidReview>(mainWindowWidget());
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

void QnWorkbenchExportHandler::showExportCompleteMessage()
{
    QnMessageBox::success(mainWindowWidget(), tr("Export completed"));
}

void QnWorkbenchExportHandler::at_camera_exportFinished(bool success, const QString &fileName)
{
    unlockFile(fileName);

    QnClientVideoCameraExportTool *tool = qobject_cast<QnClientVideoCameraExportTool*>(sender());
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
        QnMessageBox::critical(mainWindowWidget(),
            tr("Failed to export video"),
            QnStreamRecorder::errorString(tool->status()));
    }
}
