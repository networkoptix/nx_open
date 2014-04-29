#include "workbench_bookmarks_handler.h"

#include <api/app_server_connection.h>

#include <camera/loaders/caching_camera_data_loader.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_bookmark.h>

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

    QnMediaServerResourcePtr server = getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server)
        return; // TODO: #GDM show some diagnostic messages

    int handle = server->apiConnection()->addBookmarkAsync(camera, bookmark, this, SLOT(at_bookmarkAdded(int, const QnCameraBookmark &, int)));
    m_addingBookmarks[handle] = camera;
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


QnMediaServerResourcePtr QnWorkbenchBookmarksHandler::getMediaServerOnTime(const QnVirtualCameraResourcePtr &camera, qint64 time) const {
    QnMediaServerResourcePtr currentServer = qnResPool->getResourceById(camera->getParentId()).dynamicCast<QnMediaServerResource>();

    if (time == DATETIME_NOW)
        return currentServer;

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(camera->getPhysicalId());
    if (!history)
        return currentServer;

    QnTimePeriod period;
    QnMediaServerResourcePtr mediaServer = history->getMediaServerOnTime(time, true, period, false);
    if (!mediaServer)
        return currentServer;

    return mediaServer;
}

void QnWorkbenchBookmarksHandler::at_bookmarkAdded(int status, const QnCameraBookmark &bookmark, int handle) {
    QnResourcePtr camera = m_addingBookmarks.take(handle);
    if (status != 0 || !camera)
        return;

    QnCachingCameraDataLoader* loader = navigator()->loader(camera);
    if (!loader)
        return;

    //TODO: #GDM append bookmark to loader instead of forced update
    loader->forceUpdate();
}

