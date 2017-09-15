#include "workbench_export_handler.h"

#include <QtWidgets/QAction>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <core/resource_management/resource_pool.h>
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
#include <nx/client/desktop/export/tools/export_manager.h>
#include <nx/client/desktop/export/dialogs/export_settings_dialog.h>

#include <ui/dialogs/common/message_box.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_display.h>

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
    QScopedPointer<ExportManager> exportManager;

    struct ExportInfo
    {
        Filename filename;
        QPointer<QnProgressDialog> progressDialog;
    };
    QHash<QnUuid, ExportInfo> runningExports;

    WorkbenchExportHandler::Private():
        exportManager(new ExportManager())
    {
    }

    static void addResourceToPool(const Filename& filename, QnResourcePool* resourcePool)
    {
        const auto completeFilename = filename.completeFileName();
        NX_ASSERT(QFileInfo(completeFilename).exists());
        if (!QFileInfo(completeFilename).exists())
            return;

        auto existing = resourcePool->getResourceByUrl(completeFilename);
        NX_ASSERT(!existing);
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
    d(new Private())
{
    connect(d->exportManager, &ExportManager::processUpdated, this,
        &WorkbenchExportHandler::exportProcessUpdated);
    connect(d->exportManager, &ExportManager::processFinished, this,
        &WorkbenchExportHandler::exportProcessFinished);

    connect(action(ui::action::ExportVideoAction), &QAction::triggered, this,
        &WorkbenchExportHandler::handleExportVideoAction);
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
            // TODO: #GDM show full error message
            QnMessageBox::warning(mainWindow(), tr("Export failed"));
            break;
        case ExportProcessStatus::cancelled:
            break;
        default:
            NX_ASSERT(false, "Invalid state");
    }

    /*
    else if (tool->status() != StreamRecorderError::noError)
    {
        QnMessageBox::critical(mainWindow(),
            tr("Failed to export video"),
            QnStreamRecorder::errorString(tool->status()));
     *
     */

}

void WorkbenchExportHandler::handleExportVideoAction()
{
    const auto parameters = menu()->currentParameters(sender());

    const auto period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    const auto widget = extractMediaWidget(display(), parameters);
    const auto mediaResource = parameters.resource().dynamicCast<QnMediaResource>();
    NX_ASSERT(widget || mediaResource);
    if (!widget && !mediaResource)
        return;

    QScopedPointer<ExportSettingsDialog> dialog;
    dialog.reset(widget
        ? new ExportSettingsDialog(widget, period, mainWindow())
        : new ExportSettingsDialog(mediaResource, period, mainWindow()));

    if (dialog->exec() != QDialog::Accepted)
        return;

    QnUuid exportProcessId;
    Filename filename;
    switch (dialog->mode())
    {
        case ExportSettingsDialog::Mode::Media:
        {
            const auto settings = dialog->exportMediaSettings();
            exportProcessId = d->exportManager->exportMedia(settings);
            filename = settings.fileName;
            // TODO: #GDM immediately check status, export may not start in some cases.
            break;
        }
        case ExportSettingsDialog::Mode::Layout:
        {
            const auto settings = dialog->exportLayoutSettings();
            exportProcessId = d->exportManager->exportLayout(settings);
            filename = settings.filename;
            break;
        }
        default:
            NX_ASSERT(false, "Unhandled export mode");
            return;
    }

    NX_ASSERT(!exportProcessId.isNull());
    if (exportProcessId.isNull())
        return;

    auto progressDialog = new QnProgressDialog(
        filename.completeFileName(),
        tr("Stop Export"),
        0,
        100,
        mainWindow());
    connect(progressDialog, &QnProgressDialog::canceled, this,
        [this, exportProcessId]
        {
            d->exportManager->stopExport(exportProcessId);
        });

    d->runningExports.insert(exportProcessId, { filename, progressDialog });
    progressDialog->show();
    // Fill dialog with initial values (export is already running).
    exportProcessUpdated(d->exportManager->info(exportProcessId));
}

} // namespace desktop
} // namespace client
} // namespace nx
