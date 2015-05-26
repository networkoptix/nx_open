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
#include <utils/common/email.h>
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
    connect(qnLicensePool,      &QnLicensePool::licensesChanged,                            this,   &QnWorkbenchNotificationsHandler::at_licensePool_licensesChanged);

    QnCommonMessageProcessor *messageProcessor = QnCommonMessageProcessor::instance();
    connect(messageProcessor,   &QnCommonMessageProcessor::connectionOpened,                this,   &QnWorkbenchNotificationsHandler::at_eventManager_connectionOpened);
    connect(messageProcessor,   &QnCommonMessageProcessor::connectionClosed,                this,   &QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed);
    connect(messageProcessor,   &QnCommonMessageProcessor::businessActionReceived,          this,   &QnWorkbenchNotificationsHandler::at_eventManager_actionReceived);
    connect(messageProcessor,   &QnCommonMessageProcessor::timeServerSelectionRequired,     this,   &QnWorkbenchNotificationsHandler::at_timeServerSelectionRequired);

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH), &QnPropertyNotifier::valueChanged, this, &QnWorkbenchNotificationsHandler::at_settings_valueChanged);

    connect(QnGlobalSettings::instance(), &QnGlobalSettings::emailSettingsChanged,          this,   &QnWorkbenchNotificationsHandler::at_emailSettingsChanged);

    connect( action( Qn::SelectTimeServerAction ), &QAction::triggered, this, [this](){ setSystemHealthEventVisible( QnSystemHealth::NoPrimaryTimeServer, false ); } );
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler() {

}

void QnWorkbenchNotificationsHandler::clear() {
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
//    if (businessAction->actionType() != QnBusiness::ShowPopup)
//        return;

    //TODO: #GDM #Business check if camera is visible to us
    QnBusiness::UserGroup userGroup = businessAction->getParams().userGroup;
    if (userGroup == QnBusiness::AdminOnly
            && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        return;
    }

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    QnBusiness::EventType eventType = params.eventType;

    if (eventType >= QnBusiness::UserEvent)
        return;

    int healthMessage = eventType - QnBusiness::SystemHealthEvent;
    if (healthMessage >= 0) {
        QnUuid resourceId = params.eventResourceId;
        QnResourcePtr resource = qnResPool->getResourceById(resourceId);
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resource);
        return;
    }

    if (!context()->user())
        return;

    const bool soundAction = businessAction->actionType() == QnBusiness::PlaySoundAction; // TODO: #GDM #Business also PlaySoundOnceAction?
    if (!soundAction && !m_adaptor->isAllowed(eventType))
        return;

    emit businessActionAdded(businessAction);
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    addSystemHealthEvent(message, QnResourcePtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr& resource) {
    if (message == QnSystemHealth::StoragesAreFull)
        return; //Bug #2308: Need to remove notification "Storages are full"

    if (!(qnSettings->popupSystemHealth() & (1ull << message)))
        return;

    setSystemHealthEventVisible( message, resource, true );
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
    case QnSystemHealth::NoPrimaryTimeServer:
        return true;

    default:
        break;

    }
    qnWarning("Unknown system health message");
    return false;
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible) {
    setSystemHealthEventVisibleInternal(message, QVariant(), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, const QnResourcePtr &resource, bool visible) {
    setSystemHealthEventVisibleInternal( message, QVariant::fromValue( resource ), visible );
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisibleInternal( QnSystemHealth::MessageType message, const QVariant& params, bool visible ) {
    bool canShow = true;

    if (!context()->user()) {
        canShow = (message == QnSystemHealth::ConnectionLost);
        if (visible)
            Q_ASSERT_X(canShow, Q_FUNC_INFO, "No events but 'Connection lost' should be displayed if we are disconnected");
    } else {
        /* Only admins can see some system health events */
        if (adminOnlyMessage(message) && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission))
            canShow = false;
    }

    /* Checking that we want to see this message */
    bool isAllowedByFilter = qnSettings->popupSystemHealth() & (1ull << message);
    canShow &= isAllowedByFilter;

    if( visible && canShow )
        emit systemHealthEventAdded( message, params );
    else
        emit systemHealthEventRemoved( message, params );
}

void QnWorkbenchNotificationsHandler::at_context_userChanged() {
    at_licensePool_licensesChanged();

    m_adaptor->setResource(context()->user());
}

void QnWorkbenchNotificationsHandler::checkAndAddSystemHealthMessage(QnSystemHealth::MessageType message) {

    switch (message) {
    case QnSystemHealth::ConnectionLost:
    case QnSystemHealth::EmailSendError:
    case QnSystemHealth::StoragesAreFull:
        return;

    case QnSystemHealth::EmailIsEmpty:
        if (context()->user())
            m_userEmailWatcher->forceCheck(context()->user());
        return;

    case QnSystemHealth::UsersEmailIsEmpty:
        m_userEmailWatcher->forceCheckAll();
        return;

    case QnSystemHealth::NoPrimaryTimeServer:
        return;

    case QnSystemHealth::NoLicenses:
        at_licensePool_licensesChanged();
        return;

    case QnSystemHealth::SmtpIsNotSet:
        at_emailSettingsChanged();
        return;

    case QnSystemHealth::StoragesNotConfigured:
    case QnSystemHealth::ArchiveRebuildFinished:
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

void QnWorkbenchNotificationsHandler::at_timeServerSelectionRequired() {
    setSystemHealthEventVisible(QnSystemHealth::NoPrimaryTimeServer, true);
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
    {
        addBusinessAction(businessAction);
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
            addBusinessAction(businessAction);
            break;
        case QnBusiness::InactiveState:
            emit businessActionRemoved(businessAction);
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

void QnWorkbenchNotificationsHandler::at_licensePool_licensesChanged() {
    setSystemHealthEventVisible(QnSystemHealth::NoLicenses, context()->user() && qnLicensePool->isEmpty());
}

void QnWorkbenchNotificationsHandler::at_settings_valueChanged(int id) {
    if (id != QnClientSettings::POPUP_SYSTEM_HEALTH)
        return;
    quint64 filter = qnSettings->popupSystemHealth();
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        bool oldVisible = (m_popupSystemHealthFilter &  (1ull << i));
        bool newVisible = (filter &  (1ull << i));
        if (oldVisible == newVisible)
            continue;

        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        if (newVisible)
            checkAndAddSystemHealthMessage(message);
        else
            setSystemHealthEventVisible(message, false);
    }
    m_popupSystemHealthFilter = filter;
}

void QnWorkbenchNotificationsHandler::at_emailSettingsChanged() {
    QnEmailSettings settings = QnGlobalSettings::instance()->emailSettings();
    bool isInvalid = settings.server.isEmpty() || settings.user.isEmpty() || settings.password.isEmpty();
    setSystemHealthEventVisible(QnSystemHealth::SmtpIsNotSet, context()->user() && isInvalid);
}
