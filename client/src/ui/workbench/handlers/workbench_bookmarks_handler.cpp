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
#include <ui/dialogs/add_camera_bookmark_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>

QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(action(Qn::BookmarkTimeSelectionAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_bookmarkTimeSelectionAction_triggered);


    connect(resourcePool(), &QnResourcePool::resourceAdded,     this,   &QnWorkbenchBookmarksHandler::at_resPool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,   this,   &QnWorkbenchBookmarksHandler::at_resPool_resourceRemoved);
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resPool_resourceAdded(resource);

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

    QScopedPointer<QnAddCameraBookmarkDialog> dialog(new QnAddCameraBookmarkDialog(mainWindow()));
    dialog->setTagsUsage(m_tagsUsage);
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);
    camera->addBookmark(bookmark);
    QnAppServerConnectionFactory::getConnection2()->getCameraManager()->save(QnVirtualCameraResourceList() << camera, this, [](){});



}

void QnWorkbenchBookmarksHandler::at_resPool_resourceAdded(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    connect(camera, &QnVirtualCameraResource::bookmarkAdded,    this, &QnWorkbenchBookmarksHandler::at_camera_bookmarkAdded);
    connect(camera, &QnVirtualCameraResource::bookmarkChanged,  this, &QnWorkbenchBookmarksHandler::at_camera_bookmarkChanged);
    connect(camera, &QnVirtualCameraResource::bookmarkRemoved,  this, &QnWorkbenchBookmarksHandler::at_camera_bookmarkRemoved);

    foreach (const QnCameraBookmark &bookmark, camera->getBookmarks())
        at_camera_bookmarkAdded(camera, bookmark);

}

void QnWorkbenchBookmarksHandler::at_resPool_resourceRemoved(const QnResourcePtr &resource) {
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    TagsUsageHash &cameraHash = m_tagsUsageByCamera[camera];
    foreach (const QString &oldTag, cameraHash.keys())
        m_tagsUsage[oldTag] -= cameraHash[oldTag];
    m_tagsUsageByCamera.remove(camera);
}

void QnWorkbenchBookmarksHandler::at_camera_bookmarkAdded(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark) {
    TagsUsageHash &cameraHash = m_tagsUsageByCamera[camera];
    foreach (const QString &tag, bookmark.tags) {
        m_tagsUsage[tag]++;
        cameraHash[tag]++;
    }
}

void QnWorkbenchBookmarksHandler::at_camera_bookmarkChanged(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark) {
    Q_UNUSED(bookmark);
    TagsUsageHash &cameraHash = m_tagsUsageByCamera[camera];
    foreach (const QString &oldTag, cameraHash.keys())
        m_tagsUsage[oldTag] -= cameraHash[oldTag];
    cameraHash.clear();
    foreach (const QnCameraBookmark &bookmark, camera->getBookmarks()) 
        at_camera_bookmarkAdded(camera, bookmark);
}

void QnWorkbenchBookmarksHandler::at_camera_bookmarkRemoved(const QnSecurityCamResourcePtr &camera, const QnCameraBookmark &bookmark) {
    TagsUsageHash &cameraHash = m_tagsUsageByCamera[camera];
    foreach (const QString &tag, bookmark.tags) {
        m_tagsUsage[tag]--;
        cameraHash[tag]--;
    }
}
