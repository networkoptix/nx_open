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
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);
    
    QnCameraBookmark bookmark;
    bookmark.guid = QUuid::createUuid();
    bookmark.startTimeMs = period.startTimeMs;
    bookmark.durationMs = period.durationMs;
    camera->addBookmark(bookmark);

    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->save(QnVirtualCameraResourceList() << camera, this, [](){});
}