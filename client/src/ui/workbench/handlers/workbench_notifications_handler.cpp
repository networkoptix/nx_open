#include "workbench_notifications_handler.h"

#include <api/global_settings.h>
#include <api/app_server_connection.h>

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <business/business_strings_helper.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_parameters.h>
#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/resource_property_adaptors.h>
#include <utils/app_server_notification_cache.h>
#include <utils/common/warnings.h>
#include <utils/email/email.h>
#include <utils/media/audio_player.h>


QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_adaptor(new QnBusinessEventsFilterResourcePropertyAdaptor(this)),
    m_popupSystemHealthFilter(qnSettings->popupSystemHealth())
{
    m_userEmailWatcher = context()->instance<QnWorkbenchUserEmailWatcher>();
    connect(m_userEmailWatcher, &QnWorkbenchUserEmailWatcher::userEmailValidityChanged,     this,   &QnWorkbenchNotificationsHandler::at_userEmailValidityChanged);
    connect(context(),          &QnWorkbenchContext::userChanged,                           this,   &QnWorkbenchNotificationsHandler::at_context_userChanged);

    connect(qnLicensePool,      &QnLicensePool::licensesChanged,                            this,   [this] {
        checkAndAddSystemHealthMessage(QnSystemHealth::NoLicenses);
    });

    connect(qnCommon, &QnCommonModule::readOnlyChanged, this, [this] {
        checkAndAddSystemHealthMessage(QnSystemHealth::SystemIsReadOnly);
    });


    QnCommonMessageProcessor *messageProcessor = QnCommonMessageProcessor::instance();
    connect(messageProcessor,   &QnCommonMessageProcessor::connectionOpened,                this,   &QnWorkbenchNotificationsHandler::at_eventManager_connectionOpened);
    connect(messageProcessor,   &QnCommonMessageProcessor::connectionClosed,                this,   &QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed);
    connect(messageProcessor,   &QnCommonMessageProcessor::businessActionReceived,          this,   &QnWorkbenchNotificationsHandler::at_eventManager_actionReceived);

    connect(messageProcessor,   &QnCommonMessageProcessor::timeServerSelectionRequired,     this,   [this] {
        setSystemHealthEventVisible(QnSystemHealth::NoPrimaryTimeServer, true);
    });
    connect( action(QnActions::SelectTimeServerAction), &QAction::triggered,                     this,   [this] {
        setSystemHealthEventVisible( QnSystemHealth::NoPrimaryTimeServer, false );
    } );

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH), &QnPropertyNotifier::valueChanged, this, &QnWorkbenchNotificationsHandler::at_settings_valueChanged);

    connect(QnGlobalSettings::instance(), &QnGlobalSettings::emailSettingsChanged,          this,   &QnWorkbenchNotificationsHandler::at_emailSettingsChanged);
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler() {

}

void QnWorkbenchNotificationsHandler::clear() {
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addNotification(const QnAbstractBusinessActionPtr &businessAction) {
    //TODO: #GDM #Business check if camera is visible to us
    QnBusiness::UserGroup userGroup = businessAction->getParams().userGroup;
    if (userGroup == QnBusiness::AdminOnly
            && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        return;
    }

    if (businessAction->actionType() == QnBusiness::ShowOnAlarmLayoutAction) {
        QnUserResourceList users = qnResPool->getResources<QnUserResource>(businessAction->getParams().additionalResources);
        if (!users.isEmpty() && !users.contains(context()->user()))
            return;
    }

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::SystemHealthEvent && eventType <= QnBusiness::MaxSystemHealthEvent) {
        int healthMessage = eventType - QnBusiness::SystemHealthEvent;
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), businessAction);
        return;
    }

    if (!context()->user())
        return;

    bool alwaysNotify = false;
    switch (businessAction->actionType()) {
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

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    addSystemHealthEvent(message, QnAbstractBusinessActionPtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message, const QnAbstractBusinessActionPtr &businessAction)
{
    if (message == QnSystemHealth::StoragesAreFull)
        return; //Bug #2308: Need to remove notification "Storages are full"

    if (!(qnSettings->popupSystemHealth() & (1ull << message)))
        return;

    setSystemHealthEventVisibleInternal( message, QVariant::fromValue( businessAction), true);
}

bool QnWorkbenchNotificationsHandler::adminOnlyMessage(QnSystemHealth::MessageType message) {
    switch(message) {

    case QnSystemHealth::EmailIsEmpty:
    case QnSystemHealth::ConnectionLost:
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
        return true;

    default:
        break;
    }

    NX_ASSERT(false, Q_FUNC_INFO, "Unknown system health message");
    return true;
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible) {
    setSystemHealthEventVisibleInternal(message, QVariant(), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message,
                                                                  const QnResourcePtr &resource,
                                                                  bool visible)
{
    setSystemHealthEventVisibleInternal( message, QVariant::fromValue( resource ), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisibleInternal( QnSystemHealth::MessageType message,
                                                                          const QVariant& params,
                                                                          bool visible)
{
    bool canShow = true;

    bool connected = !qnCommon->remoteGUID().isNull();

    if (!connected) {
        canShow = (message == QnSystemHealth::ConnectionLost);
        if (visible) {
            /* In unit tests there can be users when we are disconnected. */
            QGuiApplication* guiApp = qobject_cast<QGuiApplication*>(qApp);
            if (guiApp)
                NX_ASSERT(canShow, Q_FUNC_INFO, "No events but 'Connection lost' should be displayed if we are disconnected");
        }
    } else {
        /* Only admins can see some system health events */
        if (adminOnlyMessage(message) && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission))
            canShow = false;
    }

    /* Some messages are not to be displayed to users. */
    canShow &= (QnSystemHealth::isMessageVisible(message));

    /* Checking that we want to see this message */
    bool isAllowedByFilter = qnSettings->popupSystemHealth() & (1ull << message);
    canShow &= isAllowedByFilter;

    if( visible && canShow )
        emit systemHealthEventAdded( message, params );
    else
        emit systemHealthEventRemoved( message, params );
}

void QnWorkbenchNotificationsHandler::at_context_userChanged() {
    m_adaptor->setResource(context()->user());

    checkAndAddSystemHealthMessage(QnSystemHealth::NoLicenses);
    checkAndAddSystemHealthMessage(QnSystemHealth::SystemIsReadOnly);
}

void QnWorkbenchNotificationsHandler::checkAndAddSystemHealthMessage(QnSystemHealth::MessageType message) {

    switch (message) {
    case QnSystemHealth::ConnectionLost:
    case QnSystemHealth::EmailSendError:
    case QnSystemHealth::StoragesAreFull:
    case QnSystemHealth::NoPrimaryTimeServer:
        return;

    case QnSystemHealth::SystemIsReadOnly:
        setSystemHealthEventVisible(QnSystemHealth::SystemIsReadOnly, context()->user() && qnCommon->isReadOnly());
        return;

    case QnSystemHealth::EmailIsEmpty:
        if (context()->user())
            m_userEmailWatcher->forceCheck(context()->user());
        return;

    case QnSystemHealth::UsersEmailIsEmpty:
        m_userEmailWatcher->forceCheckAll();
        return;

    case QnSystemHealth::NoLicenses:
        setSystemHealthEventVisible(QnSystemHealth::NoLicenses, context()->user() && qnLicensePool->isEmpty());
        return;

    case QnSystemHealth::SmtpIsNotSet:
        at_emailSettingsChanged();
        return;

    case QnSystemHealth::StoragesNotConfigured:
    case QnSystemHealth::ArchiveRebuildFinished:
    case QnSystemHealth::ArchiveRebuildCanceled:
        return;

    default:
        break;

    }
    qnWarning("Unknown system health message");
}

void QnWorkbenchNotificationsHandler::at_userEmailValidityChanged(const QnUserResourcePtr &user, bool isValid) {
    bool visible = !isValid;
    if (context()->user() == user)
        setSystemHealthEventVisible(QnSystemHealth::EmailIsEmpty, user, visible);
    else {
        /* Checking that we are allowed to see this message */
        if (visible) {
            // usual admins can not edit other admins, owner can
            if ((accessController()->globalPermissions(user) & Qn::GlobalProtectedPermission) &&
                (!(accessController()->globalPermissions() & Qn::GlobalEditProtectedUserPermission)))
                visible = false;
        }
        setSystemHealthEventVisible( QnSystemHealth::UsersEmailIsEmpty, user, visible );
    }
}


void QnWorkbenchNotificationsHandler::at_eventManager_connectionOpened() {
    setSystemHealthEventVisible(QnSystemHealth::ConnectionLost, false);
}

void QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed() {
    clear();
    if (!qnCommon->remoteGUID().isNull())
        setSystemHealthEventVisible(QnSystemHealth::ConnectionLost, true);
}

void QnWorkbenchNotificationsHandler::at_eventManager_actionReceived(const QnAbstractBusinessActionPtr &businessAction) {
    switch (businessAction->actionType()) {
    case QnBusiness::ShowPopupAction:
    case QnBusiness::ShowOnAlarmLayoutAction:
    {
        addNotification(businessAction);
        break;
    }
    case QnBusiness::PlaySoundOnceAction:
    {
        QString filename = businessAction->getParams().soundUrl;
        QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
        // if file is not exists then it is already deleted or just not downloaded yet
        // I think it should not be played when downloaded
        AudioPlayer::playFileAsync(filePath);
        break;
    }
    case QnBusiness::PlaySoundAction:
    {
        switch (businessAction->getToggleState()) {
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

void QnWorkbenchNotificationsHandler::at_settings_valueChanged(int id) {
    if (id != QnClientSettings::POPUP_SYSTEM_HEALTH)
        return;

    quint64 filter = qnSettings->popupSystemHealth();
    for (int i = 0; i < QnSystemHealth::Count; i++) {
        QnSystemHealth::MessageType messageType = static_cast<QnSystemHealth::MessageType>(i);
        if (!QnSystemHealth::isMessageVisible(messageType))
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

void QnWorkbenchNotificationsHandler::at_emailSettingsChanged() {
    QnEmailSettings settings = QnGlobalSettings::instance()->emailSettings();
    bool isInvalid = settings.server.isEmpty() || settings.user.isEmpty() || settings.password.isEmpty();
    setSystemHealthEventVisible(QnSystemHealth::SmtpIsNotSet, context()->user() && isInvalid);
}
