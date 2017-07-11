#include "workbench_notifications_handler.h"

#include <QtGui/QGuiApplication>

#include <QtWidgets/QAction>

#include <api/global_settings.h>
#include <api/app_server_connection.h>
#include <business/business_resource_validation.h>
#include <client/client_settings.h>
#include <client/client_globals.h>
#include <client/client_message_processor.h>
#include <client/client_show_once_settings.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/user_roles_manager.h>
#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_state_manager.h>
#include <utils/resource_property_adaptors.h>
#include <utils/common/warnings.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>
#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/utils/server_notification_cache.h>
#include <nx/vms/event/strings_helper.h>
#include <nx/vms/event/actions/common_action.h>
#include <ui/dialogs/camera_bookmark_dialog.h>
#include <camera/camera_bookmarks_manager.h>

using namespace nx;
using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

static const QString kCloudPromoShowOnceKey(lit("CloudPromoNotification"));

} // namespace

using namespace nx::vms::event;

QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this)),
    m_popupSystemHealthFilter(qnSettings->popupSystemHealth())
{
    m_userEmailWatcher = context()->instance<QnWorkbenchUserEmailWatcher>();

    auto sessionDelegate = new QnBasicWorkbenchStateDelegate<QnWorkbenchNotificationsHandler>(this);
    static_cast<void>(sessionDelegate); //< Debug?

    //TODO: #GDM #future
    /*
     * Some messages must be displayed before bunch of 'user email is invalid'.
     * Correct approach is to extend QnNotificationListWidget functionality with reordering.
     * Postponed to the future.
     */
    connect(m_userEmailWatcher, &QnWorkbenchUserEmailWatcher::userEmailValidityChanged,
        this, &QnWorkbenchNotificationsHandler::at_userEmailValidityChanged, Qt::QueuedConnection);

    connect(context(), &QnWorkbenchContext::userChanged,
        this, &QnWorkbenchNotificationsHandler::at_context_userChanged);

    connect(licensePool(), &QnLicensePool::licensesChanged, this,
        [this]
        {
            checkAndAddSystemHealthMessage(QnSystemHealth::NoLicenses);
        });

    connect(commonModule(), &QnCommonModule::readOnlyChanged, this,
        [this]
        {
            checkAndAddSystemHealthMessage(QnSystemHealth::SystemIsReadOnly);
        });

    QnCommonMessageProcessor* messageProcessor = qnCommonMessageProcessor;
    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
        &QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed);
    connect(messageProcessor, &QnCommonMessageProcessor::businessActionReceived, this,
        &QnWorkbenchNotificationsHandler::at_eventManager_actionReceived);

    connect(messageProcessor, &QnCommonMessageProcessor::timeServerSelectionRequired, this,
        [this]
        {
            setSystemHealthEventVisible(QnSystemHealth::NoPrimaryTimeServer, true);
        });

    connect(action(action::SelectTimeServerAction), &QAction::triggered, this,
        [this]
        {
            setSystemHealthEventVisible(QnSystemHealth::NoPrimaryTimeServer, false);
        });

    connect(action(action::HideCloudPromoAction), &QAction::triggered, this,
        [this]
        {
            qnClientShowOnce->setFlag(kCloudPromoShowOnceKey);
        });

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH),
        &QnPropertyNotifier::valueChanged, this,
        &QnWorkbenchNotificationsHandler::at_settings_valueChanged);

    connect(qnGlobalSettings, &QnGlobalSettings::emailSettingsChanged, this,
        &QnWorkbenchNotificationsHandler::at_emailSettingsChanged);

    connect(action(action::AcknowledgeEventAction), &QAction::triggered,
        this, &QnWorkbenchNotificationsHandler::handleAcknowledgeEventAction);
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler()
{
}

void QnWorkbenchNotificationsHandler::handleAcknowledgeEventAction()
{
    const auto actionParams = menu()->currentParameters(sender());
    const auto businessAction =
        actionParams.argument<vms::event::AbstractActionPtr>(Qn::ActionDataRole);

    const QScopedPointer<QnCameraBookmarkDialog> bookmarksDialog(
        new QnCameraBookmarkDialog(businessAction, mainWindow()));

    if (bookmarksDialog->exec() != QDialog::Accepted)
        return;

    const auto bookmarks = bookmarksDialog->bookmarks();

    const auto repliesRemaining = QSharedPointer<int>(new int(bookmarks.size()));
    const auto anySuccessReply = QSharedPointer<bool>(new bool(false));

    const auto creationCallback =
        [this, businessAction, repliesRemaining, anySuccessReply](bool success)
        {
            if (success)
                *anySuccessReply = true;

            auto& counter = *repliesRemaining;
            if (--counter || !*anySuccessReply)
                return;

            const auto action = CommonAction::createBroadcastAction(
                ActionType::hidePopupAction, businessAction->getParams());
            commonModule()->currentServer()->apiConnection()->broadcastAction(action);
            emit notificationRemoved(businessAction);
        };

    const auto action = CommonAction::createCopy(ActionType::bookmarkAction, businessAction);
    for (const auto& bookmark: bookmarks)
        qnCameraBookmarksManager->addAcknowledge(bookmark, action, creationCallback);
}

void QnWorkbenchNotificationsHandler::clear()
{
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addNotification(const vms::event::AbstractActionPtr &action)
{
    vms::event::EventParameters params = action->getRuntimeParams();
    vms::event::EventType eventType = params.eventType;

    const bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    if (!isAdmin)
    {
        if (eventType == vms::event::licenseIssueEvent || eventType == vms::event::networkIssueEvent)
            return;
    }

    if (eventType >= vms::event::systemHealthEvent && eventType <= vms::event::maxSystemHealthEvent)
    {
        int healthMessage = eventType - vms::event::systemHealthEvent;
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), action);
        return;
    }

    if (!context()->user())
        return;

    if (action->actionType() == vms::event::showOnAlarmLayoutAction)
    {
        /* Skip action if it contains list of users, and we are not on the list. */
        if (!QnBusiness::actionAllowedForUser(action->getParams(), context()->user()))
            return;
    }

    bool alwaysNotify = false;
    switch (action->actionType())
    {
        case vms::event::showOnAlarmLayoutAction:
        case vms::event::playSoundAction:
            //case vms::event::playSoundOnceAction: -- handled outside without notification
            alwaysNotify = true;
            break;

        default:
            break;
    }

    if (!alwaysNotify && !m_adaptor->isAllowed(eventType))
        return;

    emit notificationAdded(action);
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message)
{
    addSystemHealthEvent(message, vms::event::AbstractActionPtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message, const vms::event::AbstractActionPtr &action)
{
    if (message == QnSystemHealth::StoragesAreFull)
        return; //Bug #2308: Need to remove notification "Storages are full"

    if (!(qnSettings->popupSystemHealth() & (1ull << message)))
        return;

    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(action), true);
}

bool QnWorkbenchNotificationsHandler::tryClose(bool /*force*/)
{
    clear();
    return true;
}

void QnWorkbenchNotificationsHandler::forcedUpdate()
{
    checkAndAddSystemHealthMessage(QnSystemHealth::CloudPromo); //must be displayed first
    checkAndAddSystemHealthMessage(QnSystemHealth::NoLicenses);
    checkAndAddSystemHealthMessage(QnSystemHealth::SmtpIsNotSet);
    checkAndAddSystemHealthMessage(QnSystemHealth::SystemIsReadOnly);
    checkAndAddSystemHealthMessage(QnSystemHealth::SmtpIsNotSet);
}

bool QnWorkbenchNotificationsHandler::adminOnlyMessage(QnSystemHealth::MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::EmailIsEmpty:
            return false;

        case QnSystemHealth::NoLicenses:
        case QnSystemHealth::SmtpIsNotSet:
        case QnSystemHealth::UsersEmailIsEmpty:
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
        case QnSystemHealth::ArchiveFastScanFinished:
        case QnSystemHealth::NoPrimaryTimeServer:
        case QnSystemHealth::SystemIsReadOnly:
        case QnSystemHealth::CloudPromo:
            return true;

        default:
            break;
    }

    NX_ASSERT(false, Q_FUNC_INFO, "Unknown system health message");
    return true;
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant(), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message,
    const QnResourcePtr &resource,
    bool visible)
{
    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(resource), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisibleInternal(QnSystemHealth::MessageType message,
    const QVariant& params,
    bool visible)
{
    bool canShow = true;

    bool connected = !commonModule()->remoteGUID().isNull();

    if (!connected)
    {
        canShow = false;
        if (visible)
        {
            /* In unit tests there can be users when we are disconnected. */
            QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);
            if (guiApp)
                NX_ASSERT(false, Q_FUNC_INFO, "No events should be displayed if we are disconnected");
        }
    }
    else
    {
        /* Only admins can see some system health events */
        if (adminOnlyMessage(message) && !accessController()->hasGlobalPermission(Qn::GlobalAdminPermission))
            canShow = false;
    }

    /* Some messages are not to be displayed to users. */
    canShow &= (QnSystemHealth::isMessageVisible(message));

    /* Checking that we want to see this message */
    bool isAllowedByFilter = qnSettings->popupSystemHealth() & (1ull << message);
    canShow &= isAllowedByFilter;

    if (visible && canShow)
        emit systemHealthEventAdded(message, params);
    else
        emit systemHealthEventRemoved(message, params);
}

void QnWorkbenchNotificationsHandler::at_context_userChanged()
{
    m_adaptor->setResource(context()->user());

    forcedUpdate();
}

void QnWorkbenchNotificationsHandler::checkAndAddSystemHealthMessage(QnSystemHealth::MessageType message)
{
    switch (message)
    {
        case QnSystemHealth::EmailSendError:
        case QnSystemHealth::StoragesAreFull:
        case QnSystemHealth::NoPrimaryTimeServer:
        case QnSystemHealth::StoragesNotConfigured:
        case QnSystemHealth::ArchiveRebuildFinished:
        case QnSystemHealth::ArchiveRebuildCanceled:
            return;

        case QnSystemHealth::SystemIsReadOnly:
            setSystemHealthEventVisible(message, context()->user() && commonModule()->isReadOnly());
            return;

        case QnSystemHealth::EmailIsEmpty:
            if (context()->user())
                m_userEmailWatcher->forceCheck(context()->user());
            return;

        case QnSystemHealth::UsersEmailIsEmpty:
            m_userEmailWatcher->forceCheckAll();
            return;

        case QnSystemHealth::NoLicenses:
            setSystemHealthEventVisible(message, context()->user() && licensePool()->isEmpty());
            return;

        case QnSystemHealth::SmtpIsNotSet:
            at_emailSettingsChanged();
            return;

        case QnSystemHealth::CloudPromo:
        {
            const bool isOwner = context()->user()
                && context()->user()->userRole() == Qn::UserRole::Owner;

            const bool canShow =
                // show only to owners
                isOwner
                // only if system is not connected to the cloud
                && qnGlobalSettings->cloudSystemId().isNull()
                // and if user did not close notification manually at least once
                && !qnClientShowOnce->testFlag(kCloudPromoShowOnceKey);

            setSystemHealthEventVisible(message, canShow);
            return;
        }
        default:
            break;

    }
    qnWarning("Unknown system health message");
}

void QnWorkbenchNotificationsHandler::at_userEmailValidityChanged(const QnUserResourcePtr &user,
    bool isValid)
{
    /* Some checks are required as we making this call via queued connection. */
    if (!context()->user())
        return;

    /* Checking that we are allowed to see this message */
    bool visible = !isValid && accessController()->hasPermissions(user, Qn::WriteEmailPermission);
    auto message = context()->user() == user
        ? QnSystemHealth::EmailIsEmpty
        : QnSystemHealth::UsersEmailIsEmpty;

    setSystemHealthEventVisible(message, user, visible);
}

void QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed()
{
    clear();
}

void QnWorkbenchNotificationsHandler::at_eventManager_actionReceived(
    const vms::event::AbstractActionPtr& action)
{
    if (!QnBusiness::actionAllowedForUser(action->getParams(), context()->user()))
        return;

    switch (action->actionType())
    {
        case vms::event::hidePopupAction:
            qDebug() << "------------- hide popup" << action->getParams().actionId;
            emit notificationRemoved(action);
            break;
        case vms::event::showPopupAction:
        case vms::event::showOnAlarmLayoutAction:
        {
            addNotification(action);
            break;
        }

        case vms::event::playSoundOnceAction:
        {
            QString filename = action->getParams().url;
            QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(filename);
            // If file doesn't exist then it's already deleted or not downloaded yet.
            // I think it should not be played when downloaded.
            AudioPlayer::playFileAsync(filePath);
            break;
        }

        case vms::event::playSoundAction:
        {
            switch (action->getToggleState())
            {
                case vms::event::EventState::active:
                    addNotification(action);
                    break;

                case vms::event::EventState::inactive:
                    emit notificationRemoved(action);
                    break;

                default:
                    break;
            }
            break;
        }

        case vms::event::sayTextAction:
        {
            AudioPlayer::sayTextAsync(action->getParams().sayText);
            break;
        }

        default:
            break;
    }
}

void QnWorkbenchNotificationsHandler::at_settings_valueChanged(int id)
{
    if (id != QnClientSettings::POPUP_SYSTEM_HEALTH)
        return;

    quint64 filter = qnSettings->popupSystemHealth();
    for (int i = 0; i < QnSystemHealth::Count; i++)
    {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageOptional(messageType))
            continue;

        bool oldVisible = (m_popupSystemHealthFilter &  (1ull << i));
        bool newVisible = (filter &  (1ull << i));
        if (oldVisible == newVisible)
            continue;

        if (newVisible)
            checkAndAddSystemHealthMessage(messageType);
        else
            setSystemHealthEventVisible(messageType, false);
    }
    m_popupSystemHealthFilter = filter;
}

void QnWorkbenchNotificationsHandler::at_emailSettingsChanged()
{
    QnEmailSettings settings = qnGlobalSettings->emailSettings();
    setSystemHealthEventVisible(QnSystemHealth::SmtpIsNotSet, context()->user() && !settings.isValid());
}
