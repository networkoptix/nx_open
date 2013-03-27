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

namespace BusinessActionParameters {
    static QLatin1String emailAddress("emailAddress");

    QString getEmailAddress(const QnBusinessParams &params) {
        return params.value(emailAddress, QString()).toString();
    }

    void setEmailAddress(QnBusinessParams* params, const QString &value) {
        (*params)[emailAddress] = value;
    }

}

QnSendMailBusinessAction::QnSendMailBusinessAction(const QnBusinessParams &runtimeParams):
    base_type(BusinessActionType::SendMail, runtimeParams)
{
}

QString QnSendMailBusinessAction::getSubject() const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(m_runtimeParams);
    QString resourceName = resourceString(false);

    if (eventType >= BusinessEventType::UserDefined)
        return QObject::tr("User Defined Event (%1) has occured").arg((int)eventType - (int)BusinessEventType::UserDefined);

    switch (eventType) {
    case BusinessEventType::NotDefined:
        qWarning() << "Undefined event has occured" << m_runtimeParams;
        break;

    case BusinessEventType::Camera_Disconnect:
        return QObject::tr("Camera %1 has disconnected").arg(resourceName);

    case BusinessEventType::Camera_Input:
        return QObject::tr("Input signal caught on camera %1").arg(resourceName);

    case BusinessEventType::Camera_Motion:
        return QObject::tr("Motion was detected on camera %1").arg(resourceName);

    case BusinessEventType::Storage_Failure:
        return QObject::tr("Storage Failure at %1").arg(resourceName);

    case BusinessEventType::Network_Issue:
        return QObject::tr("Network Issue at %1").arg(resourceName);

    case BusinessEventType::MediaServer_Failure:
        return QObject::tr("Media Server %1 Failure").arg(resourceName);

    case BusinessEventType::Camera_Ip_Conflict:
        return QObject::tr("Camera IP Conflict at %1").arg(resourceName);

    case BusinessEventType::MediaServer_Conflict:
        return QObject::tr("Media Server %1 Conflict").arg(resourceName);

    default:
        break;
    }
    return QObject::tr("Unknown Event has occured");
}

QString QnSendMailBusinessAction::getMessageBody() const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(m_runtimeParams);
    QString resourceName = resourceString(true);
    QString serverName = QObject::tr("%1 Server").arg(QLatin1String(VER_COMPANYNAME_STR));

    QString messageBody;
    if (eventType >= BusinessEventType::UserDefined)
        messageBody = QObject::tr("User Defined Event (%1) has occured on %2")
                .arg((int)eventType - (int)BusinessEventType::UserDefined)
                .arg(serverName);

    switch (eventType) {
    case BusinessEventType::NotDefined:
        qWarning() << "Undefined event has occured" << m_runtimeParams;
        break;

    case BusinessEventType::Camera_Disconnect:
        return QObject::tr("%1 has detected that camera %2 was disconnected")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::Camera_Input:
        return QObject::tr("%1 has detected input signal on camera %2")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::Camera_Motion:
        return QObject::tr("%1 has detected motion on camera %2")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::Storage_Failure:
        return QObject::tr("%1 %2 has detected storage failure")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::Network_Issue:
        return QObject::tr("%1 %2 has experienced a network issue")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::MediaServer_Failure:
        return QObject::tr("%1 %2 failure was detected")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::Camera_Ip_Conflict:
        return QObject::tr("%1 %2 has detected camera IP conflict")
                .arg(serverName)
                .arg(resourceName);

    case BusinessEventType::MediaServer_Conflict:
        return QObject::tr("%1 %2 is conflicting with other server")
                .arg(serverName)
                .arg(resourceName);

    default:
        break;
    }
    return QObject::tr("Unknown Event has occured on").arg(serverName);


    if (m_aggregationInfo.totalCount() == 0) {
        messageBody += eventTextString(eventType, m_runtimeParams);
        messageBody += timestampString(m_runtimeParams, getAggregationCount());
    }
    foreach (QnInfoDetail detail, m_aggregationInfo.toList()) {
        messageBody += eventTextString(eventType, detail.runtimeParams);
        messageBody += timestampString(detail.runtimeParams, detail.count);
    }

    return messageBody;
}

void QnSendMailBusinessAction::setAggregationInfo(const QnBusinessAggregationInfo &info) {
    m_aggregationInfo = info;
}

QString QnSendMailBusinessAction::eventTextString(BusinessEventType::Value eventType, const QnBusinessParams &params) const {
    QString result;
    switch (eventType) {
    case BusinessEventType::NotDefined:
        qWarning() << "Undefined event has occured";
        return QString();

    case BusinessEventType::Camera_Disconnect:
        break;
    case BusinessEventType::Camera_Motion:
        break;
    case BusinessEventType::Camera_Input:
        {
            result += QObject::tr("Input port: %1")
                    .arg(QnBusinessEventRuntime::getInputPortId(params));
            result += QLatin1Char('\n');
        }
        break;
    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::MediaServer_Failure:
        result += reasonString(params);
        break;
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Conflict:
        result += conflictString();
        break;
    default:
        break;
    }
    return result;
}

QString QnSendMailBusinessAction::resourceString(bool useUrl) const {
    QString result;
    int id = QnBusinessEventRuntime::getEventResourceId(m_runtimeParams);
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    if (res) {
        if (useUrl)
            result = QString(QLatin1String("%1 (%2)")).arg(res->getName()).arg(res->getUrl());
        else
            result = res->getName();
    }
    return result;
}

QString QnSendMailBusinessAction::timestampString(const QnBusinessParams &params, int aggregationCount) const {
    quint64 ts = QnBusinessEventRuntime::getEventTimestamp(params);
    QString timeStamp = QDateTime::fromMSecsSinceEpoch(ts/1000).toString(Qt::SystemLocaleShortDate);

    QString result;
    int count = qMax(aggregationCount, 1);
    if (count == 1)
        result = QObject::tr("at %1").arg(timeStamp);
    else
        result = QObject::tr("%n times since %1", "", count).arg(timeStamp);
    result += QLatin1Char('\n');
    return result;
}

QString QnSendMailBusinessAction::reasonString(const QnBusinessParams &params) const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(params);
    QnBusiness::EventReason reasonCode = QnBusinessEventRuntime::getReasonCode(params);
    QString reasonText = QnBusinessEventRuntime::getReasonText(params);

    QString result;
    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (eventType == BusinessEventType::Network_Issue)
                result = QObject::tr("No video frame received during last %1 seconds")
                                                  .arg(reasonText);
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            if (eventType == BusinessEventType::Network_Issue)
                result = QObject::tr("Connection to camera was unexpectedly closed");
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            if (eventType == BusinessEventType::Network_Issue) {
                QStringList seqs = reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                result = QObject::tr("RTP packet loss detected. Prev seq.=%1 next seq.=%2")
                        .arg(seqs[0])
                        .arg(seqs[1]);
            }
            break;
        case QnBusiness::MServerIssueTerminated:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QObject::tr("Server terminated");
            break;
        case QnBusiness::MServerIssueStarted:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QObject::tr("Server started after crash");
            break;
        case QnBusiness::StorageIssueIoError:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QObject::tr("Error while writing to %1")
                                                  .arg(reasonText);
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QObject::tr("Not enough HDD/SSD speed for recording at %1")
                                                  .arg(reasonText);
            break;
        default:
            break;
    }

    if (!result.isEmpty())
        result += QLatin1Char('\n');
    return result;
}

QString QnSendMailBusinessAction::conflictString() const {
    QString source = QnBusinessEventRuntime::getSource(m_runtimeParams);
    QStringList conflicts = QnBusinessEventRuntime::getConflicts(m_runtimeParams);

    QString result = source;
    result += QLatin1Char('\n');
    result += QObject::tr("conflicted with");
    result += QLatin1Char('\n');
    foreach(QString entity, conflicts)
        result += entity + QLatin1Char('\n');
    return result;
}
