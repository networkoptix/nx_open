
#include "workbench_export_handler.h"

#include <QtWidgets/QAction>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/resource_directory_browser.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/fusion/model_functions.h>

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/storage/file_storage/layout_storage_resource.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>

#include <nx/client/desktop/export/data/export_media_settings.h>
#include <nx/client/desktop/export/tools/export_layout_tool.h>
#include <nx/client/desktop/export/tools/export_media_tool.h>
#include <nx/client/desktop/export/tools/export_manager.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/graphics/items/resource/media_resource_widget.h>

#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_item.h>
#include <ui/workbench/workbench_access_controller.h>

#ifdef Q_OS_WIN
#   include <launcher/nov_launcher_win.h>
#endif

namespace nx {
namespace client {
namespace desktop {

namespace
{

QnMediaResourceWidget* extractMediaWidget(QnWorkbenchDisplay* display,
    const ui::action::Parameters& parameters)
{
    if (parameters.size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(parameters.widget());

    if ((parameters.size() == 0) && display->widgets().size() == 1)
        return dynamic_cast<QnMediaResourceWidget *>(display->widgets().front());

    return dynamic_cast<QnMediaResourceWidget *>(display->activeWidget());
}

} // namespace

struct WorkbenchExportHandler::Private
{
    WorkbenchExportHandler* const q;
    QScopedPointer<ExportManager> exportManager;

    struct ExportInfo
    {
        Filename filename;
        QPointer<QnProgressDialog> progressDialog;
    };
    QHash<QnUuid, ExportInfo> runningExports;

    explicit Private(WorkbenchExportHandler* owner):
        q(owner),
        exportManager(new ExportManager())
    {
    }

    bool isInRunningExports(const Filename& filename) const
    {
        for (const auto& running: runningExports)
        {
            if (running.filename == filename)
                return true;
        }
        return false;
    }

    QnUuid initExport(const Filename& fileName)
    {
        const auto exportProcessId = QnUuid::createUuid();

        const auto progressDialog = new QnProgressDialog(
            fileName.completeFileName(),
            tr("Stop Export"),
            0,
            100,
            q->mainWindow());

        connect(progressDialog, &QnProgressDialog::canceled, exportManager.data(),
            [this, exportProcessId, progressDialog]
            {
                exportManager->stopExport(exportProcessId);
                progressDialog->hide();
            });

        runningExports.insert(exportProcessId, { fileName, progressDialog });

        return exportProcessId;
    }

    static void addResourceToPool(const Filename& filename, QnResourcePool* resourcePool)
    {
        const auto completeFilename = filename.completeFileName();
        NX_ASSERT(QFileInfo(completeFilename).exists());
        if (!QFileInfo(completeFilename).exists())
            return;

        const auto existing = resourcePool->getResourceByUrl(completeFilename);
        if (existing)
            resourcePool->removeResource(existing);

        switch (filename.extension)
        {
            case FileExtension::avi:
            case FileExtension::mkv:
            case FileExtension::mp4:
            {
                QnAviResourcePtr file(new QnAviResource(completeFilename));
                file->setStatus(Qn::Online);
                resourcePool->addResource(file);
                break;
            }

            case FileExtension::nov:
            case FileExtension::exe64:
            case FileExtension::exe86:
            {
                const auto layout = QnResourceDirectoryBrowser::layoutFromFile(
                    completeFilename, resourcePool);
                if (layout)
                    resourcePool->addResource(layout);
                break;
            }
            default:
                NX_ASSERT(false, "Unsuported format");
                break;
        }
    }
};


WorkbenchExportHandler::WorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    d(new Private(this))
{
    connect(d->exportManager, &ExportManager::processUpdated, this,
        &WorkbenchExportHandler::exportProcessUpdated);
    connect(d->exportManager, &ExportManager::processFinished, this,
        &WorkbenchExportHandler::exportProcessFinished);

    connect(action(ui::action::ExportVideoAction), &QAction::triggered, this,
        &WorkbenchExportHandler::handleExportVideoAction);

    connect(action(ui::action::ExportStandaloneClientAction), &QAction::triggered, this,
        &WorkbenchExportHandler::at_exportStandaloneClientAction_triggered);
}

WorkbenchExportHandler::~WorkbenchExportHandler()
{
}

void WorkbenchExportHandler::exportProcessUpdated(const ExportProcessInfo& info)
{
    if (auto dialog = d->runningExports.value(info.id).progressDialog)
    {
        dialog->setMinimum(info.rangeStart);
        dialog->setMaximum(info.rangeEnd);
        dialog->setValue(info.progressValue);
    }
}

void WorkbenchExportHandler::exportProcessFinished(const ExportProcessInfo& info)
{
    if (!d->runningExports.contains(info.id))
        return;

    const auto exportProcess = d->runningExports.take(info.id);

    if (auto dialog = exportProcess.progressDialog)
    {
        dialog->hide();
        dialog->deleteLater();
    }

    switch (info.status)
    {
        case ExportProcessStatus::success:
            d->addResourceToPool(exportProcess.filename, resourcePool());
            QnMessageBox::success(mainWindow(), tr("Export completed"));
            break;
        case ExportProcessStatus::failure:
            QnMessageBox::critical(mainWindow(), tr("Export failed"),
                ExportProcess::errorString(info.error));
            break;
        case ExportProcessStatus::cancelled:
            break;
        default:
            NX_ASSERT(false, "Invalid state");
    }
}

void WorkbenchExportHandler::handleExportVideoAction()
{
    const auto parameters = menu()->currentParameters(sender());
    const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    const auto bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);

    const auto widget = extractMediaWidget(display(), parameters);
    const auto mediaResource = parameters.resource().dynamicCast<QnMediaResource>();
    NX_ASSERT(widget || mediaResource);

    if (!widget && !mediaResource) //< Media resource or widget is mandatory.
        return;

    NX_ASSERT(period.isValid() || bookmark.isValid());
    if (!period.isValid() && !bookmark.isValid())
        return;

    const auto isFileNameValid =
        [this](const Filename& filename, bool quiet) -> bool
        {
            if (d->isInRunningExports(filename))
            {
                if (!quiet)
                {
                    QnMessageBox::warning(mainWindow(), tr("Cannot write file"),
                        tr("%1 is in use by another export.", "%1 is file name")
                            .arg(filename.completeFileName()));
                }
                return false;
            }

            if (QFileInfo::exists(filename.completeFileName()))
            {
                if (quiet || !QnFileMessages::confirmOverwrite(mainWindow(), filename.completeFileName()))
                    return false;
            }

            return true;
        };

    QWidget* main = mainWindow();
    QScopedPointer<ExportSettingsDialog> dialog;

    const auto centralResource = widget ? widget->resource() : mediaResource;

    bool hasPermission = accessController()->hasPermissions(centralResource->toResourcePtr(),
        Qn::ExportPermission);
    if (!hasPermission)
    {
        if (!widget || !period.isValid())
            return;

        // We have no permission for camera, but still can view the rest of layout
        dialog.reset(new ExportSettingsDialog(period, QnCameraBookmark(), isFileNameValid, main));

        static const QString reason = tr("Selected period cannot be exported for the current camera.");
        dialog->disableTab(ExportSettingsDialog::Mode::Media, reason);
        dialog->setLayout(widget->item()->layout()->resource());
    }
    else if (widget)
    {
        QnLayoutItemData itemData = widget->item()->data();
        // Exporting from the scene and timeline
        if (bookmark.isValid())
        {
            dialog.reset(new ExportSettingsDialog(
                { bookmark.startTimeMs, bookmark.durationMs }, bookmark, isFileNameValid, main));

            dialog->setMediaParams(centralResource, itemData, widget->context());
            dialog->hideTab(ExportSettingsDialog::Mode::Layout);
        }
        else
        {
            dialog.reset(new ExportSettingsDialog(period, bookmark, isFileNameValid, main));
            dialog->setMediaParams(centralResource, itemData, widget->context());

            // Why symmetry with the next block is broken?
            const auto periods = parameters.argument<QnTimePeriodList>(Qn::MergedTimePeriodsRole);
            bool layoutExportAllowed = periods.intersects(period);
            if (layoutExportAllowed)
                dialog->setLayout(widget->item()->layout()->resource());
            else
                dialog->hideTab(ExportSettingsDialog::Mode::Layout);
        }
    }
    else
    {
        if (bookmark.isValid())
        {
            dialog.reset(new ExportSettingsDialog(
                { bookmark.startTimeMs, bookmark.durationMs }, bookmark, isFileNameValid, main));
        }
        else
        {
            dialog.reset(new ExportSettingsDialog(period, QnCameraBookmark(), isFileNameValid, main));
        }
        dialog->setMediaParams(centralResource, QnLayoutItemData(), context());
        dialog->hideTab(ExportSettingsDialog::Mode::Layout);
    }

    if (dialog->exec() != QDialog::Accepted)
        return;

    QnUuid exportProcessId;
    std::unique_ptr<AbstractExportTool> exportTool;

    switch (dialog->mode())
    {
        case ExportSettingsDialog::Mode::Media:
        {
            const auto settings = dialog->exportMediaSettings();
            exportProcessId = d->initExport(settings.fileName);

            if (FileExtensionUtils::isLayout(settings.fileName.extension))
            {
                ExportLayoutSettings layoutSettings;
                layoutSettings.filename = settings.fileName;
                const auto& resourcePtr = mediaResource->toResourcePtr();
                layoutSettings.layout = QnLayoutResource::createFromResource(resourcePtr);
                layoutSettings.mode = ExportLayoutSettings::Mode::Export;
                layoutSettings.period = period;
                layoutSettings.readOnly = false;

                // Forcing camera rotation to match a rotation, used for camera in export preview.
                // This rotation properly matches either to:
                //  - export from the scene, uses rotation from the layout.
                //  - export from bookmark. Matches rotation from camera settings.
                auto layoutItems = layoutSettings.layout->getItems();
                if (!layoutItems.empty())
                {
                    QnLayoutItemData item = *layoutItems.begin();
                    item.rotation = settings.transcodingSettings.rotation;
                    layoutSettings.layout->updateItem(item);
                }

                exportTool.reset(new ExportLayoutTool(layoutSettings));
            }
            else
            {
                exportTool.reset(new ExportMediaTool(settings));
            }

            break;
        }
        case ExportSettingsDialog::Mode::Layout:
        {
            const auto settings = dialog->exportLayoutSettings();
            exportProcessId = d->initExport(settings.filename);
            exportTool.reset(new ExportLayoutTool(settings));
            break;
        }
        default:
            NX_ASSERT(false, "Unhandled export mode");
            return;
    }

    NX_ASSERT(exportTool);
    NX_ASSERT(!exportProcessId.isNull(), "Workflow is broken");
    if (exportProcessId.isNull())
        return;

    d->exportManager->startExport(exportProcessId, std::move(exportTool));

    const auto info = d->exportManager->info(exportProcessId);

    switch(info.status)
    {

        case ExportProcessStatus::initial:
        case ExportProcessStatus::exporting:
        case ExportProcessStatus::cancelling:
            if (const auto dialog = d->runningExports.value(exportProcessId).progressDialog)
                dialog->show();
            // Fill dialog with initial values (export is already running).
            exportProcessUpdated(info);
            break;
        case ExportProcessStatus::success:
        case ExportProcessStatus::failure:
            // Possibly export is finished already.
            exportProcessFinished(info);
            break;
        default:
            NX_ASSERT(false, "Should never get here in 'cancelled' state");
            break;
    }
}


void WorkbenchExportHandler::at_exportStandaloneClientAction_triggered()
{
#ifdef Q_OS_WIN
    const auto exeExtension = lit(".exe");
    const auto tmpExtension = lit(".tmp");

    QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
        mainWindow(),
        lit("Export Standalone Client"),
        QnAppInfo::clientExecutableName(),
        lit("*") + exeExtension
    ));
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    if (!dialog->exec())
        return;

    QString targetFilename = dialog->selectedFile();
    if (!targetFilename.endsWith(exeExtension))
        targetFilename = targetFilename +exeExtension;

    QString temporaryFilename = targetFilename;
    temporaryFilename.replace(exeExtension, tmpExtension);
    if (QnNovLauncher::createLaunchingFile(temporaryFilename) != QnNovLauncher::ErrorCode::Ok)
    {
        QnMessageBox::critical(mainWindow(), lit("File %1 cannot be written").arg(temporaryFilename));
        return;
    }

    QFile::rename(temporaryFilename, targetFilename);
    QnMessageBox::success(mainWindow(), tr("Export completed"));
#endif
}

} // namespace desktop
} // namespace client
} // namespace nx
