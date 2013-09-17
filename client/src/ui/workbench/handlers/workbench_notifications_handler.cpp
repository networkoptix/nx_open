#include "workbench_notifications_handler.h"

#include <client/client_settings.h>
#include <client_message_processor.h>

#include <business/business_strings_helper.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/email.h>

QnShowBusinessEventsHelper::QnShowBusinessEventsHelper(QObject *parent) :
    base_type(QnResourcePtr(),
              QLatin1String("showBusinessEvents"),
              0xFFFFFFFFFFFFFFFFull,
              parent)
{
}

QnShowBusinessEventsHelper::~QnShowBusinessEventsHelper(){}


QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    m_showBusinessEventsHelper = context()->instance<QnShowBusinessEventsHelper>();
    m_showBusinessEventsHelper->setResource(context()->user());

    m_userEmailWatcher = context()->instance<QnWorkbenchUserEmailWatcher>();
    connect(m_userEmailWatcher, SIGNAL(userEmailValidityChanged(const QnUserResourcePtr &, bool)),
            this,               SLOT(at_userEmailValidityChanged(const QnUserResourcePtr &, bool)));

    connect(context(),          SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
    connect(qnLicensePool,      SIGNAL(licensesChanged()), this, SLOT(at_licensePool_licensesChanged()));

    connect(QnClientMessageProcessor::instance(), SIGNAL(connectionOpened()),
            this, SLOT(at_eventManager_connectionOpened()));
    connect(QnClientMessageProcessor::instance(), SIGNAL(connectionClosed()),
            this, SLOT(at_eventManager_connectionClosed()));

    connect(qnSettings->notifier(QnClientSettings::POPUP_SYSTEM_HEALTH), SIGNAL(valueChanged(int)), this, SLOT(at_settings_valueChanged(int)));
}

QnWorkbenchNotificationsHandler::~QnWorkbenchNotificationsHandler() {

}

void QnWorkbenchNotificationsHandler::clear() {
    emit cleared();
}

void QnWorkbenchNotificationsHandler::addBusinessAction(const QnAbstractBusinessActionPtr &businessAction) {
    if (businessAction->actionType() != BusinessActionType::ShowPopup)
        return;

    //TODO: #GDM check if camera is visible to us
    QnBusinessActionParameters::UserGroup userGroup = businessAction->getParams().getUserGroup();
    if (userGroup == QnBusinessActionParameters::AdminOnly
            && !(accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        qDebug() << "popup for admins received, we are not admin";
        return;
    }

    QnBusinessEventParameters params = businessAction->getRuntimeParams();
    BusinessEventType::Value eventType = params.getEventType();

    if (eventType >= BusinessEventType::UserDefined)
        return;

    int healthMessage = eventType - BusinessEventType::SystemHealthMessage;
    if (healthMessage >= 0) {
        int resourceId = params.getEventResourceId();
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::AllResources);
        addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resource);
        return;
    }

    if (!(m_showBusinessEventsHelper->value() & (1ull << eventType))) {
        qDebug() << "popup received, ignoring" << QnBusinessStringsHelper::eventName(eventType);
        return;
    }

    int id = businessAction->getRuntimeParams().getEventResourceId();
    QnResourcePtr res = qnResPool->getResourceById(id, QnResourcePool::AllResources);
    QString resource = res ? res->getName() : QString();

    qDebug() << "popup received" << eventType << QnBusinessStringsHelper::eventName(eventType) << "from" << resource << "(" << id << ")";
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
        return true;

    default:
        break;

    }
    qnWarning("Unknown system health message");
    return false;
}

void QnWorkbenchNotificationsHandler::updateSmtpSettings(int status, const QnKvPairList &settings, int handle) {
    Q_UNUSED(handle)
    if (status != 0)
        return;

    QnEmail::Settings email(settings);
    bool isInvalid = email.server.isEmpty() || email.user.isEmpty() || email.password.isEmpty();
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
    m_showBusinessEventsHelper->setResource(context()->user());

    if (accessController()->globalPermissions() & Qn::GlobalProtectedPermission) {
        QnAppServerConnectionFactory::createConnection()->getSettingsAsync(
                       this, SLOT(updateSmtpSettings(int,QnKvPairList,int)));
    }

    if (qnLicensePool->isEmpty() &&
            (accessController()->globalPermissions() & Qn::GlobalProtectedPermission)) {
        addSystemHealthEvent(QnSystemHealth::NoLicenses);
    }
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

void QnWorkbenchNotificationsHandler::at_licensePool_licensesChanged() {
    setSystemHealthEventVisible(QnSystemHealth::NoLicenses, qnLicensePool->isEmpty());
}

void QnWorkbenchNotificationsHandler::at_settings_valueChanged(int id) {
    if (id != QnClientSettings::POPUP_SYSTEM_HEALTH)
        return;
    quint64 visible = qnSettings->popupSystemHealth();
    for (int i = 0; i < QnSystemHealth::MessageTypeCount; i++) {
        if (visible & (1ull << i))
            continue;
        QnSystemHealth::MessageType message = QnSystemHealth::MessageType(i);
        emit systemHealthEventRemoved(message, QnResourcePtr());
    }
}
