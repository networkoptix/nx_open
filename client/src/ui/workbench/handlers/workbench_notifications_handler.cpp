#include "workbench_notifications_handler.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/watchers/workbench_user_email_watcher.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

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

    connect(context(),        SIGNAL(userChanged(const QnUserResourcePtr &)), this, SLOT(at_context_userChanged()));
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
        QnResourcePtr resource = qnResPool->getResourceById(resourceId, QnResourcePool::rfAllResources);
        if (resource) //all incoming systemhealth events should contain source resource
           addSystemHealthEvent(QnSystemHealth::MessageType(healthMessage), resource);
        return;
    }

    if (!(m_showBusinessEventsHelper->value() & (1 << eventType))) {
        qDebug() << "popup received, ignoring" << BusinessEventType::toString(eventType);
        return;
    }

    int id = businessAction->getRuntimeParams().getEventResourceId();
    QnResourcePtr res = qnResPool->getResourceById(id, QnResourcePool::rfAllResources);
    QString resource = res ? res->getName() : QString();

    qDebug() << "popup received" << eventType << BusinessEventType::toString(eventType) << "from" << resource << "(" << id << ")";
    emit businessActionAdded(businessAction);
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message) {
    addSystemHealthEvent(message, QnResourcePtr());
}

void QnWorkbenchNotificationsHandler::addSystemHealthEvent(QnSystemHealth::MessageType message, const QnResourcePtr& resource) {
    if (!(qnSettings->popupSystemHealth() & (1 << message)))
        return;
    emit systemHealthEventAdded(message, resource);
}

void QnWorkbenchNotificationsHandler::setSystemHealthEventVisible(QnSystemHealth::MessageType message, const QnResourcePtr &resource, bool visible) {
    bool canShow = qnSettings->popupSystemHealth() & (1 << message);
    if (visible && canShow)
        emit systemHealthEventAdded(message, resource);
    else
        emit systemHealthEventRemoved(message, resource);
}

void QnWorkbenchNotificationsHandler::at_context_userChanged() {
    m_showBusinessEventsHelper->setResource(context()->user());
}

void QnWorkbenchNotificationsHandler::at_userEmailValidityChanged(const QnUserResourcePtr &user, bool isValid) {
    if (context()->user() == user)
        setSystemHealthEventVisible(QnSystemHealth::EmailIsEmpty, user, !isValid);
    else
        setSystemHealthEventVisible(QnSystemHealth::UsersEmailIsEmpty, user, !isValid);
}

