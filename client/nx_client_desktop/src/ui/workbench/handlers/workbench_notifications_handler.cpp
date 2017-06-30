#include "workbench_notifications_handler.h"

#include <QtGui/QGuiApplication>

#include <QtWidgets/QAction>

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>
#include <client/client_show_once_settings.h>

#include <business/business_strings_helper.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_parameters.h>
#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>
#include <ui/workbench/workbench_state_manager.h>

#include <utils/resource_property_adaptors.h>
#include <nx/client/desktop/utils/server_notification_cache.h>
#include <utils/common/warnings.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>

#include <watchers/cloud_status_watcher.h>

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

static const QString kCloudPromoShowOnceKey(lit("CloudPromoNotification"));

}


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
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler()
{

}

void QnWorkbenchNotificationsHandler::clear()
{
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addNotification(const QnAbstractBusinessActionPtr &businessAction)
{
    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    const bool isAdmin = accessController()->hasGlobalPermission(Qn::GlobalAdminPermission);

    if (!isAdmin)
    {
        if (eventType == QnBusiness::LicenseIssueEvent || eventType == QnBusiness::NetworkIssueEvent)
            return;

        if (businessAction->getParams().userGroup == QnBusiness::AdminOnly)
            return;
    }

    if (eventType >= QnBusiness::SystemHealthEvent && eventType <= QnBusiness::MaxSystemHealthEvent)
    {
        int healthMessage = eventType - QnBusiness::SystemHealthEvent;
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), businessAction);
        return;
    }

    if (!context()->user())
        return;

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction)
    {
        //TODO: #GDM code duplication
        auto allowedForUser =
            [this](const std::vector<QnUuid>& ids)
            {
                if (ids.empty())
                    return true;

                auto user = context()->user();
                if (!user)
                    return false;

                if (std::find(ids.cbegin(), ids.cend(), user->getId()) != ids.cend())
                    return true;

                auto roleId = user->userRoleId();
                return !roleId.isNull()
                    && std::find(ids.cbegin(), ids.cend(), roleId) != ids.cend();
            };


        /* Skip action if it contains list of users, and we are not on the list. */
        if (!allowedForUser(businessAction->getParams().additionalResources))
            return;
    }

    bool alwaysNotify = false;
    switch (businessAction->actionType())
    {
        case QnBusiness::ShowOnAlarmLayoutAction:
        case QnBusiness::PlaySoundAction:
            //case QnBusiness::PlaySoundOnceAction: -- handled outside without notification
            alwaysNotify = true;
            break;

        default:
            break;
    }

    if (!alwaysNotify && !m_adaptor->isAllowed(eventType))
        return;

    emit notificationAdded(businessAction);
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message)
{
    addSystemHealthEvent(message, QnAbstractBusinessActionPtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message, const QnAbstractBusinessActionPtr &businessAction)
{
    if (message == QnSystemHealth::StoragesAreFull)
        return; //Bug #2308: Need to remove notification "Storages are full"

    if (!(qnSettings->popupSystemHealth() & (1ull << message)))
        return;

    setSystemHealthEventVisibleInternal(message, QVariant::fromValue(businessAction), true);
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
            const bool isLoggedIntoCloud = qnCloudStatusWatcher->status() != QnCloudStatusWatcher::LoggedOut;

            const bool canShow =
                // show only to owners
                isOwner
                // hide if we are already in the cloud
                && !isLoggedIntoCloud
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

void QnWorkbenchNotificationsHandler::at_eventManager_actionReceived(const QnAbstractBusinessActionPtr &businessAction)
{
    switch (businessAction->actionType())
    {
        case QnBusiness::ShowPopupAction:
        case QnBusiness::ShowOnAlarmLayoutAction:
        {
            addNotification(businessAction);
            break;
        }
        case QnBusiness::PlaySoundOnceAction:
        {
            QString filename = businessAction->getParams().url;
            QString filePath = context()->instance<ServerNotificationCache>()->getFullPath(filename);
            // if file is not exists then it is already deleted or just not downloaded yet
            // I think it should not be played when downloaded
            AudioPlayer::playFileAsync(filePath);
            break;
        }
        case QnBusiness::PlaySoundAction:
        {
            switch (businessAction->getToggleState())
            {
                case QnBusiness::ActiveState:
                    addNotification(businessAction);
                    break;
                case QnBusiness::InactiveState:
                    emit notificationRemoved(businessAction);
                    break;
                default:
                    break;
            }
            break;
        }
        case QnBusiness::SayTextAction:
        {
            AudioPlayer::sayTextAsync(businessAction->getParams().sayText);
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
