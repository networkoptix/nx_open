#include "workbench_export_handler.h"

#include <QtWidgets/QAction>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>

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
    QHash<QnUuid, QPointer<QnProgressDialog>> progressDialogs;

    WorkbenchExportHandler::Private():
        exportManager(new ExportManager())
    {

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
    qDebug() << "Export process"
        << info.id.toString()
        << "state"
        << int(info.status)
        << "progress"
        << info.progressValue;

    if (auto dialog = d->progressDialogs.value(info.id))
    {
        dialog->setMinimum(info.rangeStart);
        dialog->setMaximum(info.rangeEnd);
        dialog->setValue(info.progressValue);
    }
}

void WorkbenchExportHandler::exportProcessFinished(const ExportProcessInfo& info)
{
    if (auto dialog = d->progressDialogs.value(info.id))
    {
        dialog->hide();
        dialog->deleteLater();
    }
    d->progressDialogs.remove(info.id);

    switch (info.status)
    {
        case ExportProcessStatus::success:
            // TODO: #GDM add resource to the pool
            QnMessageBox::success(mainWindow(), tr("Export completed"));
            break;
        case ExportProcessStatus::failure:
            QnMessageBox::warning(mainWindow(), tr("Export failed"));
            break;
        case ExportProcessStatus::cancelled:
            break;
        default:
            NX_ASSERT(false, "Invalid state");
    }

    /*
     *        QnAviResourcePtr file(new QnAviResource(fileName));
        file->setStatus(Qn::Online);
        resourcePool()->addResource(file);
        showExportCompleteMessage();
    }
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
    QString title;
    switch (dialog->mode())
    {
        case ExportSettingsDialog::Mode::Media:
        {
            exportProcessId = d->exportManager->exportMedia(dialog->exportMediaSettings());
            // TODO: #ynikitenkov immediately check status, export may not start in some cases.
            break;
        }
        case ExportSettingsDialog::Mode::Layout:
        {
            exportProcessId = d->exportManager->exportLayout(dialog->exportLayoutSettings());
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
        title,
        tr("Stop Export"),
        0,
        100,
        mainWindow());
    connect(progressDialog, &QnProgressDialog::canceled, this,
        [this, exportProcessId]
        {
            d->exportManager->stopExport(exportProcessId);
        });

    d->progressDialogs.insert(exportProcessId, progressDialog);
    progressDialog->show();
    // Fill dialog with initial values (export is already running).
    exportProcessUpdated(d->exportManager->info(exportProcessId));
}

} // namespace desktop
} // namespace client
} // namespace nx
