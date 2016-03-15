#include "workbench_export_handler.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QComboBox>

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
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/message_box.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>
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
#include <utils/common/environment.h>
#include <utils/common/string.h>

#include <utils/common/app_info.h>

namespace {

    /** Maximum sane duration: 30 minutes of recording. */
    static const qint64 maxRecordingDurationMsec = 1000 * 60 * 30;

    static const QString filterSeparator(QLatin1String(";;"));
}

// -------------------------------------------------------------------------- //
// QnTimestampsCheckboxControlDelegate
// -------------------------------------------------------------------------- //
class QnTimestampsCheckboxControlDelegate: public QnAbstractWidgetControlDelegate {
public:
    QnTimestampsCheckboxControlDelegate(const QString &target, QObject *parent = NULL):
        QnAbstractWidgetControlDelegate(parent),
        m_target(target){ }

    ~QnTimestampsCheckboxControlDelegate() {}

    void updateWidget(const QString &value) override {
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
    connect(action(QnActions::ExportTimeSelectionAction), &QAction::triggered, this,   &QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered);
    connect(action(QnActions::ExportLayoutAction),        &QAction::triggered, this,   &QnWorkbenchExportHandler::at_exportLayoutAction_triggered);
}

QString QnWorkbenchExportHandler::binaryFilterName() const {
#ifdef Q_OS_WIN
    #ifdef Q_OS_WIN64
        return tr("Executable %1 Media File (x64) (*.exe)").arg(QnAppInfo::organizationName());
    #else
        return tr("Executable %1 Media File (x86) (*.exe)").arg(QnAppInfo::organizationName());
    #endif
#endif
    return QString();
}


bool QnWorkbenchExportHandler::lockFile(const QString &filename) {
    if (m_filesIsUse.contains(filename)) {
        QnMessageBox::critical(
            mainWindow(),
            tr("File is in use."),
            tr("File '%1' is used for recording already. Please enter another name.").arg(QFileInfo(filename).completeBaseName()),
            QDialogButtonBox::Ok
        );
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename)) {
        QnMessageBox::critical(
            mainWindow(),
            tr("Could not overwrite file"),
            tr("File '%1' is used by another process. Please enter another name.").arg(QFileInfo(filename).completeBaseName()),
            QDialogButtonBox::Ok
        );
        return false;
    }

    m_filesIsUse << filename;
    return true;
}

void QnWorkbenchExportHandler::unlockFile(const QString &filename) {
    m_filesIsUse.remove(filename);
}

bool QnWorkbenchExportHandler::isBinaryExportSupported() const {
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
                                                     const char *slot) {
    if (!validateItemTypes(layout))
        return false;

    QnProgressDialog* exportProgressDialog = new QnProgressDialog(mainWindow(), Qt::Dialog);

    if(!cancellable) {
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
    connect(tool,                   SIGNAL(rangeChanged(int, int)), exportProgressDialog,   SLOT(setRange(int,int)));
    connect(tool,                   SIGNAL(valueChanged(int)),      exportProgressDialog,   SLOT(setValue(int)));
    connect(tool,                   SIGNAL(stageChanged(QString)),  exportProgressDialog,   SLOT(setLabelText(QString)));

    connect(tool,                   &QnLayoutExportTool::finished,  exportProgressDialog,   &QWidget::hide);
    connect(tool,                   &QnLayoutExportTool::finished,  exportProgressDialog,   &QObject::deleteLater);
    connect(tool,                   &QnLayoutExportTool::finished,  this,                   &QnWorkbenchExportHandler::at_layout_exportFinished);
    connect(tool,                   &QnLayoutExportTool::finished,  tool,                   &QObject::deleteLater);

    connect(exportProgressDialog,   SIGNAL(canceled()),             tool,                   SLOT(stop()));
    if (target && slot)
        connect(tool,               SIGNAL(finished(bool,QString)),         target,                 slot);

    return tool->start();
}

QnMediaResourceWidget *QnWorkbenchExportHandler::extractMediaWidget(const QnActionParameters &parameters)
{
    if(parameters.size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(parameters.widget());

    if((parameters.size() == 0) && display()->widgets().size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(display()->widgets().front());

    return dynamic_cast<QnMediaResourceWidget *>(display()->activeWidget());
}

void QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnMediaResourceWidget *widget = extractMediaWidget(parameters);
    QnMediaResourcePtr mediaResource = parameters.resource().dynamicCast<QnMediaResource>();

    /* Either resource or widget must be provided */
    if (!mediaResource && !widget)
    {
        QnMessageBox::critical(
              mainWindow()
            , tr("Unable to export file.")
            , tr("Exactly one item must be selected for export, but %n item(s) are currently selected." , "", parameters.size())
            );
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
        result.resource.path = resource->getUniqueId();
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

    exportTimeSelection(mediaResource, dataProvider, itemData, period);
}


//TODO: #GDM Monstrous function, refactor required
//TODO: #ynikitenkov refactor to use QnResourcePtr
void QnWorkbenchExportHandler::exportTimeSelection(
      const QnMediaResourcePtr &mediaResource
    , const QnAbstractStreamDataProvider *dataProvider
    , const QnLayoutItemData &itemData
    , const QnTimePeriod &period
    )
{
    bool wasLoggedIn = !context()->user().isNull();

    // TODO: #Elric implement more precise estimation
    if(period.durationMs > maxRecordingDurationMsec &&
            QnMessageBox::warning(
                mainWindow(),
                tr("Warning!"),
                tr("You are about to export a video that is longer than 30 minutes.") + L'\n'
              + tr("It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.") + L'\n'
              + tr("Do you want to continue?"),
                QDialogButtonBox::Yes | QDialogButtonBox::No,
                QDialogButtonBox::No
                ) == QDialogButtonBox::No)
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QString aviFileFilter = tr("AVI (*.avi)");
    QString mkvFileFilter = tr("Matroska (*.mkv)");

    QString allowedFormatFilter =
            aviFileFilter
            + filterSeparator
            + mkvFileFilter;

    if (isBinaryExportSupported())
        allowedFormatFilter += filterSeparator + binaryFilterName();

    QString fileName;
    QString selectedExtension;
    QString selectedFilter;
    bool binaryExport = false;
    ImageCorrectionParams contrastParams = itemData.contrastParams;
    QnItemDewarpingParams dewarpingParams = itemData.dewarpingParams;
    int rotation = itemData.rotation;
    QRectF zoomRect = itemData.zoomRect;
    qreal customAr = mediaResource->customAspectRatio();

    int timeOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->displayOffset(mediaResource);

    QString namePart = replaceNonFileNameCharacters(mediaResource->toResourcePtr()->getName(), L'_');
    QString timePart = (mediaResource->toResource()->flags() & Qn::utc)
            ? QDateTime::fromMSecsSinceEpoch(period.startTimeMs + timeOffset).toString(lit("yyyy_MMM_dd_hh_mm_ss"))
            : QTime(0, 0, 0, 0).addMSecs(period.startTimeMs).toString(lit("hh_mm_ss"));
    QString suggestion = QnEnvironment::getUniqueFileName(previousDir, namePart + lit("_") + timePart);

    Qn::Corner timestampPos = Qn::NoCorner;

    bool transcodeWarnShown = false;
    QnImageFilterHelper imageParameters;

    while (true) {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        QScopedPointer<QnCustomFileDialog> dialog(new QnWorkbenchStateDependentDialog<QnCustomFileDialog>(
            mainWindow(),
            tr("Export Video As..."),
            suggestion,
            allowedFormatFilter
        ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        setHelpTopic(dialog.data(), Qn::Exporting_Help);

        QnAbstractWidgetControlDelegate* delegate = isBinaryExportSupported()
            ? new QnTimestampsCheckboxControlDelegate(binaryFilterName(), this)
            : nullptr;

        QComboBox* comboBox = 0;
        bool transcodeCheckbox = false;
        if (mediaResource->hasVideo(dataProvider)) {
            comboBox = new QComboBox(dialog.data());
            comboBox->addItem(tr("No Timestamp"), Qn::NoCorner);
            comboBox->addItem(tr("Top Left Corner (requires transcoding)"), Qn::TopLeftCorner);
            comboBox->addItem(tr("Top Right Corner (requires transcoding)"), Qn::TopRightCorner);
            comboBox->addItem(tr("Bottom Left Corner (requires transcoding)"), Qn::BottomLeftCorner);
            comboBox->addItem(tr("Bottom Right Corner (requires transcoding)"), Qn::BottomRightCorner);

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
        selectedFilter = dialog->selectedNameFilter();
        if (fileName.isEmpty())
            return;

        binaryExport = isBinaryExportSupported()
            ? selectedFilter.contains(binaryFilterName())
            : false;

        if (comboBox)
            timestampPos = (Qn::Corner) comboBox->itemData(comboBox->currentIndex()).toInt();

        if (binaryExport) {
            transcodeCheckbox = false;
            timestampPos = Qn::NoCorner;
        }

        if (!transcodeCheckbox) {
            contrastParams.enabled = false;
            dewarpingParams.enabled = false;
            rotation = 0;
            zoomRect = QRectF();
            customAr = 0.0;
        }

        if (dialog->selectedNameFilter().contains(aviFileFilter)) {
            QnCachingCameraDataLoader* loader = context()->instance<QnCameraDataManager>()->loader(mediaResource);
            const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (dataProvider);
            if (loader && archive) {
                QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
                if (periods.size() > 1 && archive->getDPAudioLayout()->channelCount() > 0) {
                    int result = QnMessageBox::warning(
                        mainWindow(),
                        tr("AVI format is not recommended"),
                        tr("AVI format is not recommended for export of non-continuous recording when audio track is present."
                           "Do you want to continue?"),
                        QDialogButtonBox::Yes | QDialogButtonBox::No
                    );
                    if (result != QDialogButtonBox::Yes)
                        continue;
                }
            }
        }

        imageParameters.setSrcRect(zoomRect);
        imageParameters.setContrastParams(contrastParams);
        imageParameters.setDewarpingParams(mediaResource->getDewarpingParams(), dewarpingParams);
        imageParameters.setRotation(rotation);
        imageParameters.setCustomAR(customAr);
        imageParameters.setTimeCorner(timestampPos, timeOffset, 0);
        imageParameters.setVideoLayout(mediaResource->getVideoLayout());

        auto videoLayout = mediaResource->getVideoLayout();
        bool doTranscode = transcodeCheckbox ||
                           timestampPos != Qn::NoCorner ||
                           (!binaryExport && videoLayout && videoLayout->channelCount() > 1);

        if(doTranscode)
        {
            const QnVirtualCameraResourcePtr camera = mediaResource.dynamicCast<QnVirtualCameraResource>();
            if (camera && !transcodeWarnShown)
            {
                const int bigValue = std::numeric_limits<int>::max();
                for (const auto& stream: camera->mediaStreams().streams)
                {
                    auto filters = imageParameters.createFilterChain(stream.getResolution(), QSize(bigValue, bigValue));
                    const QSize resultResolution = imageParameters.updatedResolution( filters, stream.getResolution() );
                    if (resultResolution.width() > imageParameters.defaultResolutionLimit.width() ||
                        resultResolution.height() > imageParameters.defaultResolutionLimit.height())
                    {
                        transcodeWarnShown = true;
                        int result = QnMessageBox::warning(
                            mainWindow(),
                            tr("Selected format is not recommended"),
                            tr("Selected format is not recommended for this camera due to video downscaling. "
                            "We recommend to export selected video either to the '.nov' or '.exe' format. "
                            "Do you want to continue?"),
                            QDialogButtonBox::Yes | QDialogButtonBox::No
                            );
                        if (result != QDialogButtonBox::Yes)
                            return;
                        else
                            break; // do not show warning for other tracks
                    }

                }
            }
            if (!transcodeWarnShown)
            {
                transcodeWarnShown = true;
                QDialogButtonBox::StandardButton button = QnMessageBox::question(
                            mainWindow(),
                            tr("Save As"),
                            tr("You are about to export video with filters that require transcoding. This may take some time. Do you want to continue?"),
                            QDialogButtonBox::Yes | QDialogButtonBox::No,
                            QDialogButtonBox::No
                            );
                if(button != QDialogButtonBox::Yes)
                    return;
            }
        }


        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return;

        selectedExtension = dialog->selectedExtension();
        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            // method called under condition because in other case this message is popped out by the dialog itself
            if (QFile::exists(fileName)) {
                QDialogButtonBox::StandardButton button = QnMessageBox::information(
                            mainWindow(),
                            tr("Save As"),
                            tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel
                            );
                if (button == QDialogButtonBox::Cancel)
                    return;
                if (button == QDialogButtonBox::No) {
                    continue;
                }
            }
        }

        if (!lockFile(fileName)) {
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
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QnLayoutFileStorageResource::layoutPrefix() + fileName).dynamicCast<QnLayoutResource>();
        if (!existingLayout)
            existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            removeLayoutFromPool(existingLayout);

        QnLayoutResourcePtr newLayout(new QnLayoutResource(qnResTypePool));

        NX_ASSERT(!itemData.uuid.isNull(), Q_FUNC_INFO, "Make sure itemData is valid");
        newLayout->addItem(itemData);
        saveLayoutToLocalFile(newLayout, period, fileName, Qn::LayoutExport, false, true);
    }
    else
    {
        QnProgressDialog *exportProgressDialog = new QnWorkbenchStateDependentDialog<QnProgressDialog>(mainWindow());
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
            this);

        connect(exportProgressDialog,   &QnProgressDialog::canceled,    tool,                   &QnClientVideoCameraExportTool::stop);

        connect(tool,   &QnClientVideoCameraExportTool::finished,       this,                   &QnWorkbenchExportHandler::at_camera_exportFinished);
        connect(tool,   &QnClientVideoCameraExportTool::finished,       exportProgressDialog,   &QnProgressDialog::deleteLater);
        connect(tool,   &QnClientVideoCameraExportTool::rangeChanged,   exportProgressDialog,   &QnProgressDialog::setRange);
        connect(tool,   &QnClientVideoCameraExportTool::valueChanged,   exportProgressDialog,   &QnProgressDialog::setValue);

        tool->start();
        exportProgressDialog->show();
    }
}

void QnWorkbenchExportHandler::at_layout_exportFinished(bool success, const QString &filename) {
    unlockFile(filename);

    QnLayoutExportTool* tool = dynamic_cast<QnLayoutExportTool*>(sender());
    if (!tool)
        return;

    if (success) {
        if (tool->mode() == Qn::LayoutExport) {
            QnMessageBox::information(mainWindow(), tr("Export Complete"), tr("Export Successful"), QDialogButtonBox::Ok);
        }
    } else if (!tool->errorMessage().isEmpty()) {
        QnMessageBox::warning(mainWindow(), tr("Unable to export layout."), tool->errorMessage(), QDialogButtonBox::Ok);
    }
}


bool QnWorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr &layout) {
    bool hasImage = false;
    bool hasLocal = false;

    foreach (const QnLayoutItemData &item, layout->getItems()) {
        QnResourcePtr resource = qnResPool->getResourceByUniqueId(item.resource.path);
        if (!resource)
            continue;
        if( resource->getParentResource() == layout )
            continue;
        hasImage |= resource->hasFlags(Qn::still_image);
        hasLocal |= resource->hasFlags(Qn::local) || resource->getUrl().startsWith(QnLayoutFileStorageResource::layoutPrefix()); // layout item remove 'local' flag.
        if (hasImage || hasLocal)
            break;
    }

    if (hasImage) {
        QnMessageBox::critical(
            mainWindow(),
            tr("Unable to save layout."),
            tr("Current layout contains image files. Images are not allowed for Multi-Video export."),
            QDialogButtonBox::Ok
        );
        return false;
    }
    else if (hasLocal) {
        /* Always allow export selected area. */
        if (layout->getItems().size() == 1)
            return true;

        QnMessageBox::critical(
            mainWindow(),
            tr("Unable to save layout."),
            tr("Current layout contains local files. Local files are not allowed for Multi-Video export."),
            QDialogButtonBox::Ok
        );
        return false;
    }
    return true;
}

void QnWorkbenchExportHandler::removeLayoutFromPool(const QnLayoutResourcePtr &existingLayout)
{
    QnLayoutItemDataMap items = existingLayout->getItems();
    for(QnLayoutItemDataMap::iterator itr = items.begin(); itr != items.end(); ++itr)
    {
        QnLayoutItemData& item = itr.value();
        QnResourcePtr layoutRes = qnResPool->getResourceByUniqueId(item.resource.path);
        if (layoutRes)
            qnResPool->removeResource(layoutRes);
    }
    qnResPool->removeResource(existingLayout);
}

bool QnWorkbenchExportHandler::saveLocalLayout(const QnLayoutResourcePtr &layout, bool readOnly, bool cancellable, QObject *target, const char *slot) {
    return saveLayoutToLocalFile(layout, layout->getLocalRange(), layout->getUrl(), Qn::LayoutLocalSave, readOnly, cancellable, target, slot);
}

bool QnWorkbenchExportHandler::doAskNameAndExportLocalLayout(const QnTimePeriod& exportPeriod, const QnLayoutResourcePtr &layout, Qn::LayoutExportMode mode) {
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
    QString suggestion = replaceNonFileNameCharacters(baseName, QLatin1Char('_'));
    suggestion = QnEnvironment::getUniqueFileName(previousDir, QFileInfo(suggestion).baseName());

    QString fileName;
    bool readOnly = false;

    QString mediaFileFilter = tr("%1 Media File (*.nov)").arg(QnAppInfo::organizationName());
    if (isBinaryExportSupported())
        mediaFileFilter += filterSeparator + binaryFilterName();

    while (true) {
        /* Check if we were disconnected (server shut down) while the dialog was open.
         * Skip this check if we were not logged in before. */
        if (wasLoggedIn && !context()->user())
            return false;

        QScopedPointer<QnCustomFileDialog> dialog(new QnWorkbenchStateDependentDialog<QnCustomFileDialog>(
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
        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            // method called under condition because in other case this message is popped out by the dialog itself
            if (QFile::exists(fileName)) {
                QDialogButtonBox::StandardButton button = QnMessageBox::information(
                            mainWindow(),
                            tr("Save As"),
                            tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                            QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel
                            );
                if (button == QDialogButtonBox::Cancel)
                    return false;
                if (button == QDialogButtonBox::No)
                    continue;
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

    QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QnLayoutFileStorageResource::layoutPrefix() + fileName).dynamicCast<QnLayoutResource>();
    if (!existingLayout)
        existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
    if (existingLayout)
        removeLayoutFromPool(existingLayout);

    saveLayoutToLocalFile(layout, exportPeriod, fileName, mode, readOnly, true);

    return true;
}

void QnWorkbenchExportHandler::at_exportLayoutAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    bool wasLoggedIn = !context()->user().isNull();

    QnLayoutResourcePtr layout = workbench()->currentLayout()->resource();
    if (!layout)
        return;

    QnTimePeriod exportPeriod = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    if(exportPeriod.durationMs * layout->getItems().size() > 1000 * 60 * 30) { // TODO: #Elric implement more precise estimation
        int button = QnMessageBox::question(
            mainWindow(),
            tr("Warning!"),
            tr("You are about to export several videos with a total length exceeding 30 minutes.") + L'\n'
          + tr("It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.") + L'\n'
          + tr("Do you want to continue?"),
               QDialogButtonBox::Yes | QDialogButtonBox::No
            );
        if(button == QDialogButtonBox::No)
            return;
    }

    /* Check if we were disconnected (server shut down) while the dialog was open.
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    doAskNameAndExportLocalLayout(exportPeriod, layout, Qn::LayoutExport);
}


void QnWorkbenchExportHandler::at_camera_exportFinished(bool success, const QString &fileName) {
    unlockFile(fileName);

    QnClientVideoCameraExportTool *tool = qobject_cast<QnClientVideoCameraExportTool*>(sender());
    if (!tool)
        return;

    tool->deleteLater();

    if (success) {
        QnAviResourcePtr file(new QnAviResource(fileName));
        file->setStatus(Qn::Online);
        resourcePool()->addResource(file);

        QnMessageBox::information(mainWindow(), tr("Export Complete"), tr("Export Successful."), QDialogButtonBox::Ok);
    } else if (tool->status() != QnClientVideoCamera::NoError) {
        QnMessageBox::warning(mainWindow(), tr("Unable to export video."), QnClientVideoCamera::errorString(tool->status()), QDialogButtonBox::Ok);
    }
}
