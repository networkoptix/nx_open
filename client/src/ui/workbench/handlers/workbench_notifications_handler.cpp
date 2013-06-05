#include "workbench_notifications_handler.h"

#include <client/client_settings.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/kvpair_usage_helper.h>

QnWorkbenchNotificationsHandler::QnWorkbenchNotificationsHandler(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    m_showBusinessEventsHelper = new QnUint64KvPairUsageHelper(
                context()->user(),
                QLatin1String("showBusinessEvents"),            //TODO: #GDM move out common consts
                0xFFFFFFFFFFFFFFFFull,                          //TODO: #GDM move out common consts
                this);

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

void QnWorkbenchNotificationsHandler::at_context_userChanged() {
    m_showBusinessEventsHelper->setResource(context()->user());
}

