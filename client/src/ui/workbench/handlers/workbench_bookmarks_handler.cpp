#include "workbench_bookmarks_handler.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include <camera/camera_data_manager.h>
#include <camera/camera_bookmarks_manager.h>
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
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>
#include "core/resource/camera_history.h"

QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::AddCameraBookmarkAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered);
    connect(action(Qn::EditCameraBookmarkAction),   &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered);
    connect(action(Qn::RemoveCameraBookmarkAction), &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered);
}

ec2::AbstractECConnectionPtr QnWorkbenchBookmarksHandler::connection() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    //TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    /*
     * This check can be safely omitted in release - it is better to add bookmark on another server than do not
     * add bookmark at all.
     * //TODO: #GDM #bookmarks remember this when we will implement bookmarks timeout locking
     */
    if (QnAppInfo::beta()) {
        QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, period.startTimeMs);
        if (!server || server->getStatus() != Qn::Online) {
            QMessageBox::warning(mainWindow(),
                tr("Error"),
                tr("Bookmarks can only be added to an online server.")); //TODO: #Elric ec2 update text if needed
            return;
        }
    }

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.name = tr("Bookmark");
    bookmark.startTimeMs = period.startTimeMs;  //this should be assigned before loading data to the dialog
    bookmark.durationMs = period.durationMs;
    bookmark.cameraId = camera->getUniqueId();

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(mainWindow()));
    dialog->setTags(context()->instance<QnWorkbenchBookmarkTagsWatcher>()->tags());
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);

    qnCameraBookmarksManager->addCameraBookmark(bookmark);

    action(Qn::BookmarksModeAction)->setChecked(true);
}

void QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    //TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);

    QnMediaServerResourcePtr server = qnCameraHistoryPool->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server || server->getStatus() != Qn::Online) {
        QMessageBox::warning(mainWindow(),
            tr("Error"),
            tr("Bookmarks can only be edited on an online server.")); //TODO: #Elric ec2 update text if needed
        return;
    }

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(mainWindow()));
    dialog->setTags(context()->instance<QnWorkbenchBookmarkTagsWatcher>()->tags());
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);

    qnCameraBookmarksManager->updateCameraBookmark(bookmark);
}

void QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    //TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);

    const auto message = (bookmark.name.trimmed().isEmpty()
        ? tr("Are you sure you want to delete this bookmark?")
        : tr("Are you sure you want to delete bookmark \"%1\"?").arg(bookmark.name));

    if (QMessageBox::information(mainWindow(),
            tr("Confirm Deletion"), message,
            QMessageBox::Ok | QMessageBox::Cancel,
            QMessageBox::Cancel) != QMessageBox::Ok)
        return;

    qnCameraBookmarksManager->deleteCameraBookmark(bookmark.guid);
}

/*
void QnWorkbenchBookmarksHandler::at_bookmarkAdded(int status, const QnCameraBookmark &bookmark, int handle) {
    auto camera = m_processingBookmarks.take(handle);
    if (status != 0 || !camera)
        return;

    m_tags.append(bookmark.tags);
    m_tags.removeDuplicates();
    context()->navigator()->setBookmarkTags(m_tags);
}


void QnWorkbenchBookmarksHandler::at_bookmarkUpdated(int status, const QnCameraBookmark &bookmark, int handle) {
    auto camera = m_processingBookmarks.take(handle);
    if (status != 0 || !camera)
        return;

    m_tags.append(bookmark.tags);
    m_tags.removeDuplicates();
    context()->navigator()->setBookmarkTags(m_tags);

//     if (QnCachingCameraDataLoader* loader = context()->instance<QnCameraDataManager>()->loader(camera))
//         loader->updateBookmark(bookmark);
}

void QnWorkbenchBookmarksHandler::at_bookmarkDeleted(int status, const QnCameraBookmark &bookmark, int handle) {
    auto camera = m_processingBookmarks.take(handle);
    if (status != 0 || !camera)
        return;

//     if (QnCachingCameraDataLoader* loader = context()->instance<QnCameraDataManager>()->loader(camera))
//         loader->removeBookmark(bookmark);
}
*/
