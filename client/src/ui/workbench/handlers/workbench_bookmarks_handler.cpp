#include "workbench_bookmarks_handler.h"

#include <api/app_server_connection.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>

#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::BookmarkTimeSelectionAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_bookmarkTimeSelectionAction_triggered);
}


void QnWorkbenchBookmarksHandler::at_bookmarkTimeSelectionAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnActionTargetProvider *provider = menu()->targetProvider();
    if(!provider)
        return;
    parameters.setItems(provider->currentParameters(Qn::SceneScope).items());

    QnMediaResourceWidget *widget = NULL;

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
    QnVirtualCameraResourcePtr camera = widget->resource()->toResourcePtr().dynamicCast<QnVirtualCameraResource>();
    QnCameraBookmark bookmark;
    bookmark.guid = QUuid::createUuid();
    bookmark.startTime = period.startTimeMs;
    bookmark.endTime = period.endTimeMs();
    camera->addBookmark(bookmark);

    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->save(QnVirtualCameraResourceList() << camera, this, [](){});
}