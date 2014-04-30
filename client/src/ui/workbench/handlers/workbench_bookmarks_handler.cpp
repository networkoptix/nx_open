#include "workbench_bookmarks_handler.h"

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

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
    connect(action(Qn::AddCameraBookmarkAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered);
    connect(action(Qn::EditCameraBookmarkAction),   &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered);
    connect(action(Qn::RemoveCameraBookmarkAction), &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered);

    connect(context(), &QnWorkbenchContext::userChanged, this, &QnWorkbenchBookmarksHandler::updateTags);

    connect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::cameraBookmarkTagsAdded, this, [this](const QnCameraBookmarkTags &tags) {
        m_tags.append(tags);
        m_tags.removeDuplicates();
    });

    connect(QnCommonMessageProcessor::instance(), &QnCommonMessageProcessor::cameraBookmarkTagsRemoved, this, [this](const QnCameraBookmarkTags &tags) {
        for (const QString &tag: tags)
            m_tags.removeAll(tag);
    });
}


void QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered() {
    QnActionParameters parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    QnMediaServerResourcePtr server = getMediaServerOnTime(camera, period.startTimeMs);
    if (!server || server->getStatus() != QnResource::Online) {
        QMessageBox::warning(mainWindow(),
            tr("Error"),
            tr("Bookmark can only be added to an online server.")); //TODO: #Elric ec2 update text if needed
        return;
    }

    QnCameraBookmark bookmark;
    bookmark.guid = QUuid::createUuid();
    bookmark.startTimeMs = period.startTimeMs;  //this should be assigned before loading data to the dialog
    bookmark.durationMs = period.durationMs;

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(mainWindow()));
    dialog->setTags(m_tags);
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);

    int handle = server->apiConnection()->addBookmarkAsync(camera, bookmark, this, SLOT(at_bookmarkAdded(int, const QnCameraBookmark &, int)));
    m_addingBookmarks[handle] = camera;
}


void QnWorkbenchBookmarksHandler::updateTags() {
    if (!context()->user()) {
        m_tags.clear();
        return;
    }
    
    connection()->getCameraManager()->getBookmarkTags(this, [this](int reqID, ec2::ErrorCode code, const QnCameraBookmarkTags &tags) {
        Q_UNUSED(reqID);
        if (code != ec2::ErrorCode::ok)
            return;
        m_tags = tags;
    });
}

QnCameraBookmarkTags QnWorkbenchBookmarksHandler::tags() const {
    return m_tags;
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

    m_tags.append(bookmark.tags);
    m_tags.removeDuplicates();

    if (QnCachingCameraDataLoader* loader = navigator()->loader(camera))
        loader->addBookmark(bookmark);
}

ec2::AbstractECConnectionPtr QnWorkbenchBookmarksHandler::connection() const {
    return QnAppServerConnectionFactory::getConnection2();
}

void QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered() {

}

void QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered() {

}

