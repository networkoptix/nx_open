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
#include <core/resource/camera_history.h>

#include <recording/time_period.h>

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/actions/action_target_provider.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>

namespace {

    /* How long 'Press Ctrl-B' hint should be displayed. */
    const int kHintTimeoutMs = 5000;
}


QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */)
    : base_type(parent)
    , QnWorkbenchContextAware(parent)
    , m_hintDisplayed(false)
{
    connect(action(QnActions::AddCameraBookmarkAction),    &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered);
    connect(action(QnActions::EditCameraBookmarkAction),   &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered);
    connect(action(QnActions::RemoveCameraBookmarkAction), &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered);
    connect(action(QnActions::RemoveBookmarksAction),      &QAction::triggered,    this,   &QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered);
    connect(action(QnActions::BookmarksModeAction),        &QAction::toggled,      this,   &QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered);

    /* Reset hint flag for each user. */
    connect(context(), &QnWorkbenchContext::userChanged, this, [this]() { m_hintDisplayed = false; });
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
            QnMessageBox::warning(mainWindow(),
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
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Dialog must not allow to create invalid bookmarks");
    if (!bookmark.isValid())
        return;

    qnCameraBookmarksManager->addCameraBookmark(bookmark);

    action(QnActions::BookmarksModeAction)->setChecked(true);
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
        QnMessageBox::warning(mainWindow(),
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

    if (QnMessageBox::information(mainWindow(),
            tr("Confirm Deletion"), message,
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
            QDialogButtonBox::Cancel) != QDialogButtonBox::Ok)
        return;

    qnCameraBookmarksManager->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered()
{
    QnActionParameters parameters = menu()->currentParameters(sender());

    QnCameraBookmarkList bookmarks = parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);
    if (bookmarks.isEmpty())
        return;

    const auto message = tr("Are you sure you want to delete these %n bookmarks?", "", bookmarks.size());

    if (QnMessageBox::information(mainWindow(),
        tr("Confirm Deletion"), message,
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::Cancel) != QDialogButtonBox::Ok)
        return;

    for (const auto bookmark: bookmarks)
        qnCameraBookmarksManager->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered()
{
    const auto bookmarkModeAction = action(QnActions::BookmarksModeAction);
    const bool checked = bookmarkModeAction->isChecked();
    const bool enabled = bookmarkModeAction->isEnabled();

    bool canSaveBookmarksMode = true;    /// if bookmarks mode is going to be enabled than we always can store mode
    if (!checked)
    {
        const auto currentWidget = navigator()->currentWidget();
        canSaveBookmarksMode = (!currentWidget
            || !currentWidget->options().testFlag(QnResourceWidget::DisplayMotion));
    }

    if (enabled && canSaveBookmarksMode)
        workbench()->currentLayout()->setData(Qn::LayoutBookmarksModeRole, checked);

    if (checked)
        menu()->trigger(QnActions::StopSmartSearchAction, QnActionParameters(display()->widgets()));

    if (!m_hintDisplayed && enabled && checked && !navigator()->bookmarksModeEnabled())
    {
        QnGraphicsMessageBox::information(
              tr("Press %1 to search bookmarks").arg(action(QnActions::OpenBookmarksSearchAction)->shortcut().toString())
            , kHintTimeoutMs
            );
        m_hintDisplayed = true;
    }


    navigator()->setBookmarksModeEnabled(checked);
}
