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

// -------------------------------------------------------------------------- //
// WorkbenchExportHandler
// -------------------------------------------------------------------------- //
WorkbenchExportHandler::WorkbenchExportHandler(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_exportManager(new ExportManager())
{
    connect(m_exportManager, &ExportManager::processUpdated, this,
        &WorkbenchExportHandler::exportProcessUpdated);

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
        << (int)info.status
        << "progress"
        << info.progressValue;
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
    switch (dialog->mode())
    {
        case ExportSettingsDialog::Mode::Media:
        {
            exportProcessId = m_exportManager->exportMedia(dialog->exportMediaSettings());
            break;
        }
        case ExportSettingsDialog::Mode::Layout:
        {

            break;
        }
        default:
            NX_ASSERT(false, "Unhandled export mode");
            return;
    }

    NX_ASSERT(!exportProcessId.isNull());
    if (exportProcessId.isNull())
        return;

}

} // namespace desktop
} // namespace client
} // namespace nx
