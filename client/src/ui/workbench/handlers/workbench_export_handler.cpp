#include "workbench_export_handler.h"

#include <QtWidgets/QMessageBox>

#include <camera/caching_time_period_loader.h>
#include <camera/video_camera.h>

#include <client/client_settings.h>

#include <common/common_globals.h>

#include <plugins/resources/archive/archive_stream_reader.h>

#include <recording/stream_recorder.h>
#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_server_time_watcher.h>

#include <utils/common/string.h>

namespace {

    /** Maximum sane duration: 30 minutes of recording. */
    static const qint64 maxRecordingDurationMsec = 1000 * 60 * 30;

}

QnWorkbenchExportHandler::QnWorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::ExportTimeSelectionAction), SIGNAL(triggered()), this, SLOT(at_exportTimeSelectionAction_triggered()));
}

#ifdef Q_OS_WIN
QString QnWorkbenchExportHandler::binaryFilterName() const {
#ifdef Q_OS_WIN64
    return tr("Executable %1 Media File (x64) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
#else
    return tr("Executable %1 Media File (x86) (*.exe)").arg(QLatin1String(QN_ORGANIZATION_NAME));
#endif
}
#endif

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
    DewarpingParams dewarpingParams = itemData.dewarpingParams;

    while (true) {
        QString namePart = replaceNonFileNameCharacters(widget->resource()->toResourcePtr()->getName(), lit('_'));
        QString timePart = (widget->resource()->toResource()->flags() & QnResource::utc)
                ? QDateTime::fromMSecsSinceEpoch(period.startTimeMs).toString(lit("yyyy_MMM_dd_hh_mm_ss"))
                : QTime().addMSecs(period.startTimeMs).toString(lit("hh_mm_ss"));
        QString suggestion = namePart + lit("_") + timePart;

        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            mainWindow(),
            tr("Export Video As..."),
            previousDir + QDir::separator() + suggestion,
            allowedFormatFilter
        ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        QnCheckboxControlAbstractDelegate* delegate = NULL;
#ifdef Q_OS_WIN
        delegate = new QnTimestampsCheckboxControlDelegate(binaryFilterName(), this);
#endif
        QComboBox* comboBox = new QComboBox(dialog.data());
        comboBox->addItem(tr("No timestamp"), Qn::NoCorner);
        comboBox->addItem(tr("Top left corner (required transcoding)"), Qn::TopLeftCorner);
        comboBox->addItem(tr("Top right corner (required transcoding)"), Qn::TopRightCorner);
        comboBox->addItem(tr("Bottom left corner (required transcoding)"), Qn::BottomLeftCorner);
        comboBox->addItem(tr("Bottom right corner (required transcoding)"), Qn::BottomRightCorner);

        dialog->addWidget(new QLabel(tr("Timestamps:"), dialog.data()));
        dialog->addWidget(comboBox, false);


        bool doTranscode = contrastParams.enabled || dewarpingParams.enabled;
        if (doTranscode) {
            if (contrastParams.enabled && dewarpingParams.enabled) {
                dialog->addCheckBox(tr("Apply dewarping and image correction (requires transcoding)"), &doTranscode, delegate);
            } else if (contrastParams.enabled) {
                dialog->addCheckBox(tr("Apply image correction (requires transcoding)"), &doTranscode, delegate);
            } else {
                dialog->addCheckBox(tr("Apply dewarping (requires transcoding)"), &doTranscode, delegate);
            }
        }

        if (!dialog->exec() || dialog->selectedFiles().isEmpty())
            return;

        fileName = dialog->selectedFiles().value(0);
        if (fileName.isEmpty())
            return;

        timestampPos = (Qn::Corner) comboBox->itemData(comboBox->currentIndex()).toInt();

        contrastParams.enabled &= doTranscode;
        dewarpingParams.enabled &= doTranscode;
        selectedFilter = dialog->selectedNameFilter();
        selectedExtension = selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);

        if(doTranscode || timestampPos != Qn::NoCorner) {
            QMessageBox::StandardButton button = QMessageBox::question(
                        mainWindow(),
                        tr("Save As"),
                        tr("You are about to export video with filters that require transcoding. Transcoding can take a long time. Do you want to continue?"),
                        QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                        QMessageBox::No
                        );
            if(button != QMessageBox::Yes)
                return;
        }

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    mainWindow(),
                    tr("Save As"),
                    tr("File '%1' already exists. Overwrite?").arg(QFileInfo(fileName).baseName()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                );
                if(button != QMessageBox::Yes)
                    return;
            }
        }

        if (selectedFilter.contains(aviFileFilter))
        {
            QnCachingTimePeriodLoader* loader = navigator()->loader(widget->resource()->toResourcePtr());
            const QnArchiveStreamReader* archive = dynamic_cast<const QnArchiveStreamReader*> (widget->display()->dataProvider());
            if (loader && archive)
            {
                QnTimePeriodList periods = loader->periods(Qn::RecordingContent).intersected(period);
                if (periods.size() > 1 && archive->getDPAudioLayout()->channelCount() > 0)
                {
                    int result = QMessageBox::warning(
                        mainWindow(),
                        tr("AVI format is not recommended"),
                        tr("AVI format is not recommended for camera with audio track there is some recording holes exists."\
                           "Press 'Yes' to continue export or 'No' to select other format"), // TODO: #Elric bad Engrish
                        QMessageBox::Yes | QMessageBox::No
                    );
                    if (result != QMessageBox::Yes)
                        continue;
                }
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()),
                QMessageBox::Ok
            );
            continue;
        }

        break;
    }
    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());

#ifdef Q_OS_WIN
    if (selectedFilter.contains(binaryFilterName()))
    {
        QnLayoutResourcePtr existingLayout = qnResPool->getResourceByUrl(QLatin1String("layout://") + fileName).dynamicCast<QnLayoutResource>();
        if (!existingLayout)
            existingLayout = qnResPool->getResourceByUrl(fileName).dynamicCast<QnLayoutResource>();
        if (existingLayout)
            removeLayoutFromPool(existingLayout);

        QnLayoutResourcePtr newLayout(new QnLayoutResource());

        itemData.uuid = QUuid::createUuid();
        newLayout->addItem(itemData);
        saveLayoutToLocalFile(period, newLayout, fileName, LayoutExport_Export, false, true, true);
    }
    else
#endif
    {
        QnProgressDialog *exportProgressDialog = new QnProgressDialog(mainWindow());
        exportProgressDialog->setWindowTitle(tr("Exporting Video"));
        exportProgressDialog->setLabelText(tr("Exporting to \"%1\"...").arg(fileName));
        exportProgressDialog->setRange(0, 100);
        exportProgressDialog->setMinimumDuration(1000);
        exportProgressDialog->setModal(false);

        QnMediaResourcePtr resource = widget->resource();
        QnVideoCamera* camera = new QnVideoCamera(resource);

        connect(exportProgressDialog,   SIGNAL(canceled()),                 camera,                 SLOT(stopExport()));
        connect(exportProgressDialog,   SIGNAL(canceled()),                 camera,                 SLOT(deleteLater()));
        connect(exportProgressDialog,   SIGNAL(canceled()),                 exportProgressDialog,   SLOT(deleteLater()));

        connect(camera,                 SIGNAL(exportProgress(int)),        exportProgressDialog,   SLOT(setValue(int)));

        connect(camera,                 SIGNAL(exportFailed(QString)),      camera,                 SLOT(deleteLater()));
        connect(camera,                 SIGNAL(exportFailed(QString)),      exportProgressDialog,   SLOT(hide()));
        connect(camera,                 SIGNAL(exportFailed(QString)),      exportProgressDialog,   SLOT(deleteLater()));

        connect(camera,                 SIGNAL(exportFinished(QString)),    camera,                 SLOT(deleteLater()));
        connect(camera,                 SIGNAL(exportFinished(QString)),    exportProgressDialog,   SLOT(hide()));
        connect(camera,                 SIGNAL(exportFinished(QString)),    exportProgressDialog,   SLOT(deleteLater()));

        QnStreamRecorder::Role role = QnStreamRecorder::Role_FileExport;

        int timeOffset = 0;
        if(qnSettings->timeMode() == Qn::ServerTimeMode) {
            // time difference between client and server
            timeOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(resource, 0);
        }
        qint64 serverTimeZone = context()->instance<QnWorkbenchServerTimeWatcher>()->utcOffset(resource, Qn::InvalidUtcOffset);
        camera->exportMediaPeriodToFile(period.startTimeMs * 1000ll, (period.startTimeMs + period.durationMs) * 1000ll, fileName, selectedExtension.mid(1),
                                                  QnStorageResourcePtr(), role,
                                                  timestampPos,
                                                  timeOffset, serverTimeZone,
                                                  itemData.zoomRect,
                                                  contrastParams,
                                                  dewarpingParams);

        exportProgressDialog->show();
    }
}
