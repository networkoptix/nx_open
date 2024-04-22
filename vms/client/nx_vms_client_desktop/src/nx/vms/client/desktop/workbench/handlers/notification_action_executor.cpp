// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "notification_action_executor.h"

#include <api/server_rest_connection.h>
#include <camera/camera_bookmarks_manager.h>
#include <core/resource/camera_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/bookmark_models.h>
#include <nx/vms/api/rules/event_log.h>
#include <nx/vms/client/core/access/access_controller.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <nx/vms/client/desktop/rules/cross_system_notifications_listener.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/workbench/workbench.h>
#include <nx/vms/common/api/helpers/bookmark_api_converter.h>
#include <nx/vms/common/saas/saas_utils.h>
#include <nx/vms/common/user_management/user_management_helpers.h>
#include <nx/vms/rules/actions/repeat_sound_action.h>
#include <nx/vms/rules/actions/show_notification_action.h>
#include <nx/vms/rules/actions/show_on_alarm_layout_action.h>
#include <nx/vms/rules/engine.h>
#include <nx/vms/rules/ini.h>
#include <nx/vms/rules/utils/action.h>
#include <nx/vms/rules/utils/field.h>
#include <nx/vms/rules/utils/string_helper.h>
#include <nx/vms/rules/utils/type.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_display.h>
#include <utils/common/synctime.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::rules;
using namespace std::chrono;

namespace {

QnCameraBookmark bookmarkFromAction(
    const NotificationAction* action,
    const QnVirtualCameraResourcePtr& camera)
{
    if (!NX_ASSERT(camera))
        return {};

    // TODO: #amalov Fix hardcoded bookmark times.
    static const auto kDuration = 10s;

    QnCameraBookmark bookmark;

    bookmark.guid = nx::Uuid::createUuid();
    bookmark.startTimeMs = duration_cast<milliseconds>(action->timestamp());
    bookmark.durationMs = kDuration;
    bookmark.cameraId = camera->getId();
    bookmark.name = action->extendedCaption();
    if (bookmark.name.isEmpty())
        bookmark.name = action->caption();

    bookmark.description = action->description();

    return bookmark;
}

}  // namespace

NotificationActionExecutor::NotificationActionExecutor(QObject* parent):
    QnWorkbenchContextAware(parent)
{
    auto engine = systemContext()->vmsRulesEngine();

    engine->addActionExecutor(rules::utils::type<NotificationAction>(), this);
    engine->addActionExecutor(rules::utils::type<RepeatSoundAction>(), this);
    engine->addActionExecutor(rules::utils::type<ShowOnAlarmLayoutAction>(), this);

    connect(context(), &QnWorkbenchContext::userChanged,
        this, &NotificationActionExecutor::onContextUserChanged);

    connect(action(menu::AcknowledgeNotificationAction), &QAction::triggered,
        this, &NotificationActionExecutor::handleAcknowledgeAction);

}

NotificationActionExecutor::~NotificationActionExecutor()
{}

void NotificationActionExecutor::setAcknowledge(
    nx::vms::api::rules::EventLogRecordList&& records)
{
    for (const auto& record: records)
    {
        auto action = systemContext()->vmsRulesEngine()->buildAction(record.actionData)
            .dynamicCast<NotificationAction>();

        if (!action || !checkUserPermissions(context()->user(), action))
            continue;

        // TODO: #amalov Transfer source server.
        action->setServerId(record.serverId);

        emit notificationActionReceived(action, {});
    }
}

void NotificationActionExecutor::handleAcknowledgeAction()
{
    const auto actionParams = menu()->currentParameters(sender());
    const auto notification = actionParams.argument<NotificationActionPtr>(Qn::ActionDataRole);

    const auto camera = actionParams.resource().dynamicCast<QnVirtualCameraResource>();
    auto systemContext = SystemContext::fromResource(camera);

    if (!camera || !systemContext || camera->hasFlags(Qn::removed))
    {
        QnSessionAwareMessageBox::warning(
            mainWindowWidget(),
            tr("Unable to acknowledge event on removed camera."));

        removeNotification(notification);
        return;
    }

    if (!accessController()->hasPermissions(camera, Qn::ManageBookmarksPermission))
    {
        QnSessionAwareMessageBox::warning(
            mainWindowWidget(),
            tr("Unable to acknowledge event on inaccessible camera."));

        removeNotification(notification);
        return;
    }

    auto bookmark = bookmarkFromAction(notification.get(), camera);
    if (!bookmark.isValid())
        return;

    const QScopedPointer<QnCameraBookmarkDialog> bookmarksDialog(
        new QnCameraBookmarkDialog(true /*mandatoryDescription*/, mainWindowWidget()));

    bookmark.description.clear(); //< Force user to fill description out.
    bookmarksDialog->loadData(bookmark);
    if (bookmarksDialog->exec() != QDialog::Accepted)
        return;

    bookmarksDialog->submitData(bookmark);

    bookmark.creatorId = context()->user()->getId();
    bookmark.creationTimeStampMs = qnSyncTime->value();

    auto acknowledgeBookmark =
        nx::vms::common::bookmarkToApi<nx::vms::api::rules::AcknowledgeBookmark>(
            std::move(bookmark), notification->originPeerId());
    acknowledgeBookmark.eventId = notification->id();
    acknowledgeBookmark.eventServerId = notification->originPeerId();

    auto callback = nx::utils::guarded(
        this,
        [this, id = notification->id(), camera](
            rest::Handle,
            bool /*success*/,
            rest::ErrorOrData<nx::vms::api::BookmarkV3>&& response)
        {
            if (const auto error = std::get_if<nx::network::rest::Result>(&response))
            {
                NX_WARNING(this, "Can't acknowledge action id: %1, code: %2, error: %3",
                    id, error->error, error->errorString);
                return;
            }

            auto& bookmark = std::get<nx::vms::api::BookmarkV3>(response);
            NX_VERBOSE(this, "Acknowledged action id: %1, bookmark id: %2", id, bookmark.id);

            const auto systemContext = SystemContext::fromResource(camera);
            systemContext->cameraBookmarksManager()->addExistingBookmark(
                nx::vms::common::bookmarkFromApi(std::move(bookmark)));
        });

    const auto handle = systemContext->connectedServerApi()->acknowledge(
        acknowledgeBookmark,
        std::move(callback),
        this->thread());

    // Hiding notification instantly to keep UX smooth.
    if (handle)
        removeNotification(notification);
}

void NotificationActionExecutor::onContextUserChanged()
{
    using namespace nx::vms;

    const auto user = context()->user();

    if (user && user->isCloud() && common::saas::saasServicesOperational(systemContext()))
    {
        m_listener = std::make_unique<CrossSystemNotificationsListener>();
        connect(m_listener.get(),
            &CrossSystemNotificationsListener::notificationActionReceived,
            this,
            &NotificationActionExecutor::notificationActionReceived);
    }
    else
    {
        m_listener.reset();
    }
}

void NotificationActionExecutor::execute(const ActionPtr& action)
{
    if (!vms::rules::checkUserPermissions(context()->user(), action))
        return;

    auto notificationAction = action.dynamicCast<NotificationActionBase>();
    if (!NX_ASSERT(notificationAction, "Unexpected action: %1", action->type()))
        return;

    emit notificationActionReceived(notificationAction, {});

    if (notificationAction->state() == State::stopped)
        return;

    // Show splash.
    QSet<QnResourcePtr> targetResources;

    if (!notificationAction->deviceIds().empty())
    {
        targetResources = nx::utils::toQSet(
            resourcePool()->getResourcesByIds(notificationAction->deviceIds()));
    }
    else
    {
        targetResources.insert(resourcePool()->getResourceById(notificationAction->serverId()));
    }

    targetResources.remove({});

    display()->showNotificationSplash(
        targetResources.values(), QnNotificationLevel::convert(notificationAction->level()));
}

void NotificationActionExecutor::removeNotification(
    const nx::vms::rules::NotificationActionBasePtr & action)
{
    action->setState(nx::vms::rules::State::stopped);
    emit notificationActionReceived(action, {});
}

} // namespace nx::vms::client::desktop
