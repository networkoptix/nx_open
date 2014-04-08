#include "workbench_notifications_handler.h"

#include <api/app_server_connection.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <business/business_strings_helper.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/resource_property_adaptors.h>
#include <utils/app_server_notification_cache.h>
#include <utils/common/email.h>
#include <utils/media/audio_player.h>

QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent) :
    QObject(parent),
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

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH), &QnPropertyNotifier::valueChanged, this, &QnWorkbenchNotificationsHandler::at_settings_valueChanged);
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler() {

}

void QnWorkbenchNotificationsHandler::clear() {
    emit cleared();
}

void QnWorkbenchNotificationsHandler::requestSmtpSettings() {
    if (accessController()->globalPermissions() & Qn::GlobalProtectedPermission)
        QnAppServerConnectionFactory::getConnection2()->getSettingsAsync(
            this, &QnWorkbenchNotificationsHandler::updateSmtpSettings );
}

void QnWorkbenchNotificationsHandler::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
//    if (businessAction->actionType() != BusinessActionType::ShowPopup)
//        return;

    //TODO: #GDM check if camera is visible to us
    QnBusinessActionParameters::UserGroup userGroup = businessAction->getParams().getUserGroup();
    if (userGroup == QnBusinessActionParameters::AdminOnly
            && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        return;
    }

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = params.getEventType();

    if (eventType >= BusinessEventType::UserDefined)
        return;

    int healthMessage = eventType - BusinessEventType::SystemHealthMessage;
    if (healthMessage >= 0) {
        QnId resourceId = params.getEventResourceId();
        QnResourcePtr resource = qnResPool->getResourceById(resourceId);
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resource);
        return;
    }

    if (!context()->user())
        return;

    const bool soundAction = businessAction->actionType() == BusinessActionType::PlaySoundRepeated;
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

    emit systemHealthEventAdded(message, resource);
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
        return true;

    default:
        break;

    }
    qnWarning("Unknown system health message");
    return false;
}

void QnWorkbenchNotificationsHandler::updateSmtpSettings( int handle, ec2::ErrorCode errorCode, const QnEmail::Settings &settings ) {
    Q_UNUSED(handle)
    if (errorCode != ec2::ErrorCode::ok)
        return;

    bool isInvalid = settings.server.isEmpty() || settings.user.isEmpty() || settings.password.isEmpty();
    setSystemHealthEventVisible(QnSystemHealth::SmtpIsNotSet, isInvalid);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, bool visible) {
    setSystemHealthEventVisible(message, QnResourcePtr(), visible);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, const QnResourcePtr &resource, bool visible) {
    /* Only admins can see some system health events */
    if (visible && adminOnlyMessage(message) && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission))
        return;

    /* Checking that we are allowed to see this message */
    if (visible && message == QnSystemHealth::UsersEmailIsEmpty) {
        QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
        if (!user)
            return;

        // usual admins can not edit other admins, owner can
        if ((accessController()->globalPermissions(user) & Qn::GlobalProtectedPermission) &&
            (!(accessController()->globalPermissions() & Qn::GlobalEditProtectedUserPermission)))
            return;
    }

    /* Checking that we want to see this message */
    bool canShow = qnSettings->popupSystemHealth() & (1ull << message);

    if (visible && canShow)
        emit systemHealthEventAdded(message, resource);
    else
        emit systemHealthEventRemoved(message, resource);
}

void QnWorkbenchNotificationsHandler::at_context_userChanged() {
    requestSmtpSettings();
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

    case QnSystemHealth::NoLicenses:
        at_licensePool_licensesChanged();
        return;

    case QnSystemHealth::SmtpIsNotSet:
        requestSmtpSettings();
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
    if (context()->user() == user)
        setSystemHealthEventVisible(QnSystemHealth::EmailIsEmpty, user, !isValid);
    else
        setSystemHealthEventVisible(QnSystemHealth::UsersEmailIsEmpty, user, !isValid);
}

void QnWorkbenchNotificationsHandler::at_eventManager_connectionOpened() {
    setSystemHealthEventVisible(QnSystemHealth::ConnectionLost, false);
}

void QnWorkbenchNotificationsHandler::at_eventManager_connectionClosed() {
    clear();
    setSystemHealthEventVisible(QnSystemHealth::ConnectionLost, QnResourcePtr(), true);
}

void QnWorkbenchNotificationsHandler::at_eventManager_actionReceived(const QnAbstractBusinessActionPtr &businessAction) {
    switch (businessAction->actionType()) {
    case BusinessActionType::ShowPopup:
    {
        addBusinessAction(businessAction);
        break;
    }
    case BusinessActionType::PlaySound:
    {
        QString filename = businessAction->getParams().getSoundUrl();
        QString filePath = context()->instance<QnAppServerNotificationCache>()->getFullPath(filename);
        // if file is not exists then it is already deleted or just not downloaded yet
        // I think it should not be played when downloaded
        AudioPlayer::playFileAsync(filePath);
        break;
    }
    case BusinessActionType::PlaySoundRepeated:
    {
        switch (businessAction->getToggleState()) {
        case Qn::OnState:
            addBusinessAction(businessAction);
            break;
        case Qn::OffState:
            emit businessActionRemoved(businessAction);
            break;
        default:
            break;
        }
        break;
    }
    case BusinessActionType::SayText:
    {
        AudioPlayer::sayTextAsync(businessAction->getParams().getSayText());
        break;
    }
    default:
        break;
    }
}

void QnWorkbenchNotificationsHandler::at_licensePool_licensesChanged() {
    setSystemHealthEventVisible(QnSystemHealth::NoLicenses, qnLicensePool->isEmpty());
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
