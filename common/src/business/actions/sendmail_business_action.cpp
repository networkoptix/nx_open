/**********************************************************
* 29 nov 2012
* a.kolesnikov
***********************************************************/

#include "sendmail_business_action.h"

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <business/events/reasoned_business_event.h>
#include <business/events/conflict_business_event.h>
#include <business/events/camera_input_business_event.h>

#include "version.h"
#include "api/app_server_connection.h"
#include "business/business_strings_helper.h"

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::SendMail, runtimeParams)
{
}

QString QnSendMailBusinessAction::getSubject() const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(m_runtimeParams);
    QString resourceName = QnBusinessStringsHelper::resourceName(m_runtimeParams);

    if (eventType >= BusinessEventType::UserDefined)
        return QObject::tr("User Defined Event (%1) has occured").arg((int)eventType - (int)BusinessEventType::UserDefined);

    switch (eventType) {
    case BusinessEventType::NotDefined:
        return QObject::tr("Undefined event has occured");

    case BusinessEventType::Camera_Disconnect:
        return QObject::tr("Camera %1 was disconnected").arg(resourceName);

    case BusinessEventType::Camera_Input:
        return QObject::tr("Input signal was caught on camera %1").arg(resourceName);

    case BusinessEventType::Camera_Motion:
        return QObject::tr("Motion was detected on camera %1").arg(resourceName);

    case BusinessEventType::Storage_Failure:
        return QObject::tr("Storage Failure at \"%1\"").arg(resourceName);

    case BusinessEventType::Network_Issue:
        return QObject::tr("Network Issue at %1").arg(resourceName);

    case BusinessEventType::MediaServer_Failure:
        return QObject::tr("Media Server \"%1\" Failure").arg(resourceName);

    case BusinessEventType::Camera_Ip_Conflict:
        return QObject::tr("Camera IP Conflict at \"%1\"").arg(resourceName);

    case BusinessEventType::MediaServer_Conflict:
        return QObject::tr("Media Server \"%1\" Conflict").arg(resourceName);

    default:
        break;
    }
    return QObject::tr("Unknown Event has occured");
}

QString QnSendMailBusinessAction::getMessageBody() const {

    return QnBusinessStringsHelper::longEventDescription(this, m_aggregationInfo);
}

void QnSendMailBusinessAction::setAggregationInfo(const QnBusinessAggregationInfo &info) {
    m_aggregationInfo = info;
}

