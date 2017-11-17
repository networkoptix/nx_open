#include "workbench_bookmarks_handler.h"

#include <QtWidgets/QAction>

#include <api/app_server_connection.h>
#include <api/common_message_processor.h>

#include <camera/camera_data_manager.h>
#include <camera/camera_bookmarks_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_history.h>

#include <recording/time_period.h>

#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/generic/graphics_message_box.h>
#include <ui/graphics/items/controls/bookmarks_viewer.h>
#include <ui/graphics/items/controls/time_slider.h>

#include <ui/statistics/modules/controls_statistics_module.h>

#include <ui/workbench/workbench.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_display.h>
#include <ui/workbench/workbench_layout.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_navigator.h>
#include <ui/workbench/watchers/workbench_bookmark_tags_watcher.h>

#include <utils/common/app_info.h>
#include <utils/common/synctime.h>
#include <nx/client/desktop/utils/parameter_helper.h>

using namespace nx::client::desktop::ui;

namespace {

/* How long 'Press Ctrl-B' hint should be displayed. */
const int kHintTimeoutMs = 5000;
}


QnWorkbenchBookmarksHandler::QnWorkbenchBookmarksHandler(QObject *parent /* = NULL */):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_hintDisplayed(false)
{
    connect(action(action::AddCameraBookmarkAction),     &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered);
    connect(action(action::EditCameraBookmarkAction),    &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered);
    connect(action(action::RemoveCameraBookmarkAction),  &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered);
    connect(action(action::RemoveBookmarksAction),       &QAction::triggered, this,
        &QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered);
    connect(action(action::BookmarksModeAction),         &QAction::toggled,   this,
        &QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered);

    /* Reset hint flag for each user. */
    connect(context(), &QnWorkbenchContext::userChanged, this, [this]() { m_hintDisplayed = false; });

    const auto getActionParamsFunc =
        [this](const QnCameraBookmark &bookmark) -> action::Parameters
        {
            return navigator()->currentParameters(action::TimelineScope)
                .withArgument(Qn::CameraBookmarkRole, bookmark);
        };

    const QPointer<QnBookmarksViewer> bookmarksViewer(navigator()->timeSlider()->bookmarksViewer());

    const auto updateBookmarkActionsAvailability =
        [this, bookmarksViewer]()
        {
            if (!bookmarksViewer)
                return;

            const bool readonly = commonModule()->isReadOnly()
                || !accessController()->hasGlobalPermission(Qn::GlobalManageBookmarksPermission);

            bookmarksViewer->setReadOnly(readonly);
        };

    connect(accessController(), &QnWorkbenchAccessController::globalPermissionsChanged, this,
        updateBookmarkActionsAvailability);
    connect(commonModule(), &QnCommonModule::readOnlyChanged, this, updateBookmarkActionsAvailability);
    connect(context(), &QnWorkbenchContext::userChanged, this, updateBookmarkActionsAvailability);

    connect(bookmarksViewer, &QnBookmarksViewer::editBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_edit"));
            menu()->triggerIfPossible(action::EditCameraBookmarkAction,
                getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::removeBookmarkClicked, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_delete"));
            menu()->triggerIfPossible(action::RemoveCameraBookmarkAction,
                getActionParamsFunc(bookmark));
        });

    connect(bookmarksViewer, &QnBookmarksViewer::playBookmark, this,
        [this, getActionParamsFunc](const QnCameraBookmark &bookmark)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_play"));

            static const int kMicrosecondsFactor = 1000;
            navigator()->setPosition(bookmark.startTimeMs * kMicrosecondsFactor);
            navigator()->setPlaying(true);
        });

    connect(bookmarksViewer, &QnBookmarksViewer::tagClicked, this,
        [this, bookmarksViewer](const QString &tag)
        {
            context()->statisticsModule()->registerClick(lit("bookmark_tooltip_tag"));
            menu()->triggerIfPossible(action::OpenBookmarksSearchAction,
                {Qn::BookmarkTagRole, tag});
        });
}

void QnWorkbenchBookmarksHandler::at_addCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    auto camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnTimePeriod period = parameters.argument<QnTimePeriod>(Qn::TimePeriodRole);

    /*
     * This check can be safely omitted in release - it is better to add bookmark on another server than do not
     * add bookmark at all.
     * // TODO: #GDM #bookmarks remember this when we will implement bookmarks timeout locking
     */
    if (QnAppInfo::beta())
    {
        QnMediaServerResourcePtr server = cameraHistoryPool()->getMediaServerOnTime(camera, period.startTimeMs);
        if (!server || server->getStatus() != Qn::Online)
        {
            QnMessageBox::warning(mainWindow(),
                tr("Server offline"),
                tr("Bookmarks can only be added to an online server."));
            return;
        }
    }

    QnCameraBookmark bookmark;
    bookmark.guid = QnUuid::createUuid();
    bookmark.name = tr("Bookmark");
    bookmark.startTimeMs = period.startTimeMs;  //this should be assigned before loading data to the dialog
    bookmark.durationMs = period.durationMs;
    bookmark.cameraId = camera->getId();

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(false, mainWindow()));
    dialog->setTags(context()->instance<QnWorkbenchBookmarkTagsWatcher>()->tags());
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;

    bookmark.creatorId = context()->user()->getId();
    bookmark.creationTimeStampMs = qnSyncTime->currentMSecsSinceEpoch();
    dialog->submitData(bookmark);
    NX_ASSERT(bookmark.isValid(), Q_FUNC_INFO, "Dialog must not allow to create invalid bookmarks");
    if (!bookmark.isValid())
        return;

    qnCameraBookmarksManager->addCameraBookmark(bookmark);

    action(action::BookmarksModeAction)->setChecked(true);
}

void QnWorkbenchBookmarksHandler::at_editCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);

    QnMediaServerResourcePtr server = cameraHistoryPool()->getMediaServerOnTime(camera, bookmark.startTimeMs);
    if (!server || server->getStatus() != Qn::Online)
    {
        QnMessageBox::warning(mainWindow(),
            tr("Server offline"),
            tr("Bookmarks can only be edited on an online Server."));
        return;
    }

    QScopedPointer<QnCameraBookmarkDialog> dialog(new QnCameraBookmarkDialog(false, mainWindow()));
    dialog->setTags(context()->instance<QnWorkbenchBookmarkTagsWatcher>()->tags());
    dialog->loadData(bookmark);
    if (!dialog->exec())
        return;
    dialog->submitData(bookmark);

    qnCameraBookmarksManager->updateCameraBookmark(bookmark);
}

void QnWorkbenchBookmarksHandler::at_removeCameraBookmarkAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());
    QnVirtualCameraResourcePtr camera = parameters.resource().dynamicCast<QnVirtualCameraResource>();
    // TODO: #GDM #Bookmarks will we support these actions for exported layouts?
    if (!camera)
        return;

    QnCameraBookmark bookmark = parameters.argument<QnCameraBookmark>(Qn::CameraBookmarkRole);

    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Delete bookmark?"), bookmark.name.trimmed(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        mainWindow());
    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    qnCameraBookmarksManager->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_removeBookmarksAction_triggered()
{
    const auto parameters = menu()->currentParameters(sender());

    QnCameraBookmarkList bookmarks = parameters.argument<QnCameraBookmarkList>(Qn::CameraBookmarkListRole);
    if (bookmarks.isEmpty())
        return;

    const auto parent = nx::utils::extractParentWidget(parameters, mainWindow());
    QnMessageBox dialog(QnMessageBoxIcon::Question,
        tr("Delete %n bookmarks?", "", bookmarks.size()), QString(),
        QDialogButtonBox::Cancel, QDialogButtonBox::NoButton,
        parent);
    dialog.addCustomButton(QnMessageBoxCustomButton::Delete,
        QDialogButtonBox::AcceptRole, Qn::ButtonAccent::Warning);

    if (dialog.exec() == QDialogButtonBox::Cancel)
        return;

    for (const auto bookmark : bookmarks)
        qnCameraBookmarksManager->deleteCameraBookmark(bookmark.guid);
}

void QnWorkbenchBookmarksHandler::at_bookmarksModeAction_triggered()
{
    const auto bookmarkModeAction = action(action::BookmarksModeAction);
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
        menu()->trigger(action::StopSmartSearchAction, display()->widgets());

    if (!m_hintDisplayed && enabled && checked && !navigator()->bookmarksModeEnabled())
    {
        QnGraphicsMessageBox::information(
            tr("Press %1 to search bookmarks").arg(action(action::OpenBookmarksSearchAction)->shortcut().toString())
            , kHintTimeoutMs
        );
        m_hintDisplayed = true;
    }


    navigator()->setBookmarksModeEnabled(checked);
}
