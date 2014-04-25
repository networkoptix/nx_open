#include "workbench_bookmarks_handler.h"

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>

#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::BookmarkTimeSelectionAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_bookmarkTimeSelectionAction_triggered);

    connect(context(), &QnWorkbenchContext::userChanged, this, &QnWorkbenchBookmarksHandler::updateTagsUsage);
}


void QnWorkbenchBookmarksHandler::at_bookmarkTimeSelectionAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    QnCameraBookmark bookmark;
    bookmark.guid = QUuid::createUuid();
    bookmark.startTimeMs = period.startTimeMs;  //this should be assigned before loading data to the dialog
    bookmark.durationMs = period.durationMs;

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(mainWindow()));
    dialog->setTagsUsage(m_tagsUsage);
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);

    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->save(QnVirtualCameraResourceList() << camera, this, [this](){ updateTagsUsage(); });
}


void QnWorkbenchBookmarksHandler::updateTagsUsage() {
    if (!context()->user()) {
        m_tagsUsage.clear();
        return;
    }
    
    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->getBookmarkTagsUsage(this, [this](int reqID, ec2::ErrorCode code, const QnCameraBookmarkTagsUsage &usage) {
        Q_UNUSED(reqID);
        if (code != ec2::ErrorCode::ok)
            return;
        m_tagsUsage = usage;
    });
}

QnCameraBookmarkTagsUsage QnWorkbenchBookmarksHandler::tagsUsage() const {
    return m_tagsUsage;
}
