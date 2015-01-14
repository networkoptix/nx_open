#include "workbench_export_handler.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QComboBox>

#include <client/client_settings.h>

#include <common/common_globals.h>

#include <camera/loaders/caching_camera_data_loader.h>
#include <camera/client_video_camera.h>
#include <camera/client_video_camera_export_tool.h>

#include <core/resource/resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource_management/resource_pool.h>

#include <plugins/resource/archive/archive_stream_reader.h>
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/workbench_state_dependent_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/extensions/workbench_layout_export_tool.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/common/event_processors.h>
#include <utils/common/environment.h>
#include <utils/common/string.h>

#include <utils/common/app_info.h>

namespace {

    /** Maximum sane duration: 30 minutes of recording. */
    static const qint64 maxRecordingDurationMsec = 1000 * 60 * 30;
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
    connect(action(Qn::ExportTimeSelectionAction), &QAction::triggered, this,   &QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered);
    connect(action(Qn::ExportLayoutAction),        &QAction::triggered, this,   &QnWorkbenchExportHandler::at_exportLayoutAction_triggered);
}

#ifdef Q_OS_WIN
QString QnWorkbenchExportHandler::binaryFilterName() const {
#ifdef Q_OS_WIN64
    return tr("Executable %1 Media File (x64) (*.exe)").arg(QnAppInfo::organizationName());
#else
    return tr("Executable %1 Media File (x86) (*.exe)").arg(QnAppInfo::organizationName());
#endif
}
#endif

bool QnWorkbenchExportHandler::lockFile(const QString &filename) {
    if (m_filesIsUse.contains(filename)) {
        QMessageBox::critical(
            mainWindow(),
            tr("File is in use"),
            tr("File '%1' is used for recording already. Please enter another name.").arg(QFileInfo(filename).completeBaseName()),
            QMessageBox::Ok
        );
        return false;
    }

    if (QFile::exists(filename) && !QFile::remove(filename)) {
        QMessageBox::critical(
            mainWindow(),
            tr("Could not overwrite file"),
            tr("File '%1' is used by another process. Please enter another name.").arg(QFileInfo(filename).completeBaseName()),
            QMessageBox::Ok
        );
        return false;
    }

    m_filesIsUse << filename;
    return true;
}

void QnWorkbenchExportHandler::unlockFile(const QString &filename) {
    m_filesIsUse.remove(filename);
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
    exportProgressDialog->setMinimumDuration(1000);
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

void QnWorkbenchExportHandler::at_exportTimeSelectionAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;
    parameters.setItems(provider->currentParameters(Qn::SceneScope).items());

    QnMediaResourceWidget *widget = NULL;
    Qn::Corner timestampPos = Qn::NoCorner;

    if(parameters.size() != 1) {
        if(parameters.size() == 0 && display()->widgets().size() == 1) {
            widget = dynamic_cast<QnMediaResourceWidget *>(display()->widgets().front());
        } else {
            widget = dynamic_cast<QnMediaResourceWidget *>(display()->activeWidget());
            if (!widget) {
                QMessageBox::critical(
                    mainWindow(),
                    tr("Could not export file"),
                    tr("Exactly one item must be selected for export, but %n item(s) are currently selected.", "", parameters.size())
                );
                return;
            }
        }
    } else {
        widget = dynamic_cast<QnMediaResourceWidget *>(parameters.widget());
    }
    if(!widget)
        return;

    bool wasLoggedIn = !context()->user().isNull();

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    // TODO: #Elric implement more precise estimation
    if(period.durationMs > maxRecordingDurationMsec &&
            QMessageBox::warning(
                mainWindow(),
                tr("Warning"),
                tr("You are about to export a video sequence that is longer than 30 minutes.\n"
                   "It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.\n"
                   "Do you want to continue?"),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No
                ) == QMessageBox::No)
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. 
     * Skip this check if we were not logged in before. */
    if (wasLoggedIn && !context()->user())
        return;

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QString filterSeparator(QLatin1String(";;"));
    QString aviFileFilter = tr("AVI (*.avi)");
    QString mkvFileFilter = tr("Matroska (*.mkv)");

    QString allowedFormatFilter =
            aviFileFilter
            + filterSeparator
            + mkvFileFilter
#ifdef Q_OS_WIN
            + filterSeparator
            + binaryFilterName()
#endif
            ;

    QnLayoutItemData itemData = widget->item()->data();

    QString fileName;
    QString selectedExtension;
    QString selectedFilter;
    ImageCorrectionParams contrastParams = itemData.contrastParams;
    QnItemDewarpingParams dewarpingParams = itemData.dewarpingParams;
    int rotation = itemData.rotation;
    QRectF zoomRect = itemData.zoomRect;
    qreal customAr = widget->resource()->customAspectRatio();

    int timeOffset = 0;
    if (qnSettings->timeMode() == Qn::ServerTimeMode) {
        // time difference between client and server
        timeOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(widget->resource(), 0);
    }

    QString namePart = replaceNonFileNameCharacters(widget->resource()->toResourcePtr()->getName(), L'_');
    QString timePart = (widget->resource()->toResource()->flags() & Qn::utc)
            ? QDateTime::fromMSecsSinceEpoch(period.startTimeMs + timeOffset).toString(lit("yyyy_MMM_dd_hh_mm_ss"))
            : QTime(0, 0, 0, 0).addMSecs(period.startTimeMs + timeOffset).toString(lit("hh_mm_ss"));
    QString suggestion = QnEnvironment::getUniqueFileName(previousDir, namePart + lit("_") + timePart);

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

        QnAbstractWidgetControlDelegate* delegate = NULL;
#ifdef Q_OS_WIN
        delegate = new QnTimestampsCheckboxControlDelegate(binaryFilterName(), this);
#endif
        QComboBox* comboBox = new QComboBox(dialog.data());
        comboBox->addItem(tr("No timestamp"), Qn::NoCorner);
        comboBox->addItem(tr("Top left corner (requires transcoding)"), Qn::TopLeftCorner);
        comboBox->addItem(tr("Top right corner (requires transcoding)"), Qn::TopRightCorner);
        comboBox->addItem(tr("Bottom left corner (requires transcoding)"), Qn::BottomLeftCorner);
        comboBox->addItem(tr("Bottom right corner (requires transcoding)"), Qn::BottomRightCorner);

        dialog->addWidget(tr("Timestamps:"), comboBox, delegate);


        bool doTranscode = contrastParams.enabled || dewarpingParams.enabled || itemData.rotation || customAr || !zoomRect.isNull();
        if (doTranscode) 
        {
            dialog->addCheckBox(tr("Transcode video to guarantee WYSIWYG"), &doTranscode, delegate);
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

        timestampPos = (Qn::Corner) comboBox->itemData(comboBox->currentIndex()).toInt();

        if (!doTranscode) {
            contrastParams.enabled = false;
            dewarpingParams.enabled = false;
            rotation = 0;
            zoomRect = QRectF();
            customAr = 0.0;
        }

        if (dialog->selectedNameFilter().contains(aviFileFilter)) {
            QnCachingCameraDataLoader* loader = navigator()->loader(widget->resource()->toResourcePtr());
            const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (widget->display()->dataProvider());
            if (loader && archive) {
                QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
                if (periods.size() > 1 && archive->getDPAudioLayout()->channelCount() > 0) {
                    int result = QMessageBox::warning(
                        mainWindow(),
                        tr("AVI format is not recommended"),
                        tr("AVI format is not recommended for export of non-continuous recording when audio track is present."
                           "Do you want to continue?"), 
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (result != QMessageBox::Yes)
                        continue;
                }
            }
        }

        if(doTranscode || timestampPos != Qn::NoCorner) {
            QMessageBox::StandardButton button = QMessageBox::question(
                        mainWindow(),
                        tr("Save As"),
                        tr("You are about to export video with filters that require transcoding, which can take a long time. Do you want to continue?"),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                        QMessageBox::No
                        );
            if(button != QMessageBox::Yes)
                return;
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
                QMessageBox::StandardButton button = QMessageBox::information(
                            mainWindow(),
                            tr("Save As"),
                            tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                            );
                if (button == QMessageBox::Cancel)
                    return;
                if (button == QMessageBox::No) {
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

#ifdef Q_OS_WIN
    if (selectedFilter.contains(binaryFilterName()))
    {
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QnLayoutFileStorageResource::layoutPrefix() + fileName).dynamicCast<QnLayoutResource>();
        if (!existingLayout)
            existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            removeLayoutFromPool(existingLayout);

        QnLayoutResourcePtr newLayout(new QnLayoutResource(qnResTypePool));

        itemData.uuid = QnUuid::createUuid();
        newLayout->addItem(itemData);
        saveLayoutToLocalFile(newLayout, period, fileName, Qn::LayoutExport, false, true);
        return;
    }
#endif

    QnProgressDialog *exportProgressDialog = new QnWorkbenchStateDependentDialog<QnProgressDialog>(mainWindow());
    exportProgressDialog->setWindowTitle(tr("Exporting Video"));
    exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
    exportProgressDialog->setMinimumDuration(1000);
    exportProgressDialog->setModal(false);

    QnMediaResourcePtr resource = widget->resource();
    QnClientVideoCamera* camera = new QnClientVideoCamera(resource);

    qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(resource, Qn::InvalidUtcOffset);
    QnClientVideoCameraExportTool *tool = new QnClientVideoCameraExportTool(
                                              camera,
                                              period,
                                              fileName,
                                              timestampPos,
                                              timeOffset,
                                              serverTimeZone,
                                              zoomRect,
                                              contrastParams,
                                              dewarpingParams,
                                              rotation,
                                              customAr,
                                              this);

    connect(exportProgressDialog,   &QnProgressDialog::canceled,    tool,                   &QnClientVideoCameraExportTool::stop);

    connect(tool,   &QnClientVideoCameraExportTool::finished,       this,                   &QnWorkbenchExportHandler::at_camera_exportFinished);
    connect(tool,   &QnClientVideoCameraExportTool::finished,       exportProgressDialog,   &QnProgressDialog::deleteLater);
    connect(tool,   &QnClientVideoCameraExportTool::rangeChanged,   exportProgressDialog,   &QnProgressDialog::setRange);
    connect(tool,   &QnClientVideoCameraExportTool::valueChanged,   exportProgressDialog,   &QnProgressDialog::setValue);

    tool->start();
    exportProgressDialog->show();
}

void QnWorkbenchExportHandler::at_layout_exportFinished(bool success, const QString &filename) {
    unlockFile(filename);

    QnLayoutExportTool* tool = dynamic_cast<QnLayoutExportTool*>(sender());
    if (!tool)
        return;

    if (success) {
        if (tool->mode() == Qn::LayoutExport) {
            QMessageBox::information(mainWindow(), tr("Export finished"), tr("Export successfully finished"), QMessageBox::Ok);
        }
    } else if (!tool->errorMessage().isEmpty()) {
        QMessageBox::warning(mainWindow(), tr("Could not export layout"), tool->errorMessage(), QMessageBox::Ok);
    }
}


bool QnWorkbenchExportHandler::validateItemTypes(const QnLayoutResourcePtr &layout) {
    bool hasImage = false;
    bool hasLocal = false;

    foreach (const QnLayoutItemData &item, layout->getItems()) {
        QnResourcePtr resource = qnResPool->getResourceByUniqId(item.resource.path);
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
        QMessageBox::critical(
            mainWindow(),
            tr("Could not save a layout"),
            tr("Current layout contains image files. Images are not allowed for Multi-Video export."),
            QMessageBox::Ok
        );
        return false;
    }
    else if (hasLocal) {
        /* Always allow export selected area. */
        if (layout->getItems().size() == 1)
            return true;

        QMessageBox::critical(
            mainWindow(),
            tr("Could not save a layout"),
            tr("Current layout contains local files. Local files are not allowed for Multi-Video export."),
            QMessageBox::Ok
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
        QnResourcePtr layoutRes = qnResPool->getResourceByUniqId(item.resource.path);
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
        dialogName = tr("Save local layout As...");
    else if (mode == Qn::LayoutExport)
        dialogName = tr("Export Layout As...");
    else
        return false; // not used

    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();

    QString suggestion = replaceNonFileNameCharacters(layout->getName(), QLatin1Char('_'));
    suggestion = QnEnvironment::getUniqueFileName(previousDir, QFileInfo(suggestion).baseName());

    QString fileName;
    bool readOnly = false;

#ifdef Q_OS_WIN
    QString filterSeparator(QLatin1String(";;"));
#endif
    QString mediaFileFilter = tr("%1 Media File (*.nov)").arg(QnAppInfo::organizationName())
#ifdef Q_OS_WIN
            + filterSeparator
            + binaryFilterName()
#endif
            ;

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
        dialog->addCheckBox(tr("Make file read-only"), &readOnly);

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
                QMessageBox::StandardButton button = QMessageBox::information(
                            mainWindow(),
                            tr("Save As"),
                            tr("File '%1' already exists. Do you want to overwrite it?").arg(QFileInfo(fileName).completeBaseName()),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                            );
                if (button == QMessageBox::Cancel)
                    return false;
                if (button == QMessageBox::No)
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
        int button = QMessageBox::question(
            mainWindow(),
            tr("Warning"),
            tr("You are about to export several video sequences with a total length exceeding 30 minutes. \n"
               "It may require over a gigabyte of HDD space, and, depending on your connection speed, may also take several minutes to complete.\n"
               "Do you want to continue?"),
               QMessageBox::Yes | QMessageBox::No
            );
        if(button == QMessageBox::No)
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

        QMessageBox::information(mainWindow(), tr("Export finished"), tr("Export successfully finished."), QMessageBox::Ok);
    } else if (tool->status() != QnClientVideoCamera::NoError) {
        QMessageBox::warning(mainWindow(), tr("Could not export video"), QnClientVideoCamera::errorString(tool->status()), QMessageBox::Ok);
    }
}
