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

    if (eventType >= BusinessEventType::UserDefined)
        return QObject::tr("User defined event");

    QString name;
    if (BusinessEventType::isResourceRequired(eventType)) {
        name = resourceString(false);
    }

    switch (eventType) {
    case BusinessEventType::NotDefined:
        qWarning() << "Undefined event has occured";
        return QString();

    case BusinessEventType::Camera_Disconnect:
        return QObject::tr("Camera %1 disconnected").arg(name);

    case BusinessEventType::Camera_Input:
        return QObject::tr("Input signal on camera %1").arg(name);

    case BusinessEventType::Camera_Motion:
        return QObject::tr("Motion detected on camera %1").arg(name);

    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::MediaServer_Failure:

        break;
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Conflict:

        break;
    default:
        break;
    }
    return QString();
}

QString QnSendMailBusinessAction::getMessageBody() const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(m_runtimeParams);

    QString messageBody = BusinessEventType::toString(eventType);
    messageBody += QLatin1Char('\n');

    if (eventType >= BusinessEventType::UserDefined)
        return messageBody;

    if (BusinessEventType::isResourceRequired(eventType))
        messageBody += resourceString(true);

    switch (eventType) {
    case BusinessEventType::NotDefined:
        qWarning() << "Undefined event has occured";
        return QString();

    case BusinessEventType::Camera_Disconnect:
    case BusinessEventType::Camera_Motion:
        messageBody += stateString();
        break;
    case BusinessEventType::Camera_Input:
        {
            messageBody += QObject::tr("Input port: %1")
                    .arg(BusinessEventParameters::getInputPortId(m_runtimeParams));
            messageBody += QLatin1Char('\n');
            messageBody += stateString();
        }
        break;
    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::MediaServer_Failure:
        messageBody += reasonString();
        break;
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Conflict:
        messageBody += conflictString();
        break;
    default:
        return QString();
    }
    messageBody += QLatin1Char('\n');

    messageBody += timestampString();
    messageBody += adresatesString();
    return messageBody;
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

    if (!result.isEmpty())
        result += QLatin1Char('\n');
    return result;
}


QString QnSendMailBusinessAction::timestampString() const {
    quint64 ts = QnBusinessEventRuntime::getEventTimestamp(m_runtimeParams);
    QString timeStamp = QDateTime::fromMSecsSinceEpoch(ts/1000).toString(Qt::SystemLocaleShortDate);

    QString result;
    int count = qMax(getAggregationCount(), 1);
    if (count == 1)
        result = QObject::tr("at %1").arg(timeStamp);
    else
        result = QObject::tr("%1 times since %2").arg(count).arg(timeStamp);
    result += QLatin1Char('\n');
    return result;
}


QString QnSendMailBusinessAction::adresatesString() const {
    QString result = QObject::tr("Adresates:");
    result += QLatin1Char('\n');
    QnResourceList resources = getResources();
    foreach (const QnUserResourcePtr &user, resources.filtered<QnUserResource>())
        result += user->getName() + QLatin1Char('\n');

    QString additional = BusinessActionParameters::getEmailAddress(getParams());
    QStringList receivers = additional.split(QLatin1Char(';'), QString::SkipEmptyParts);
    foreach (const QString &receiver, receivers)
        result += receiver.trimmed() + QLatin1Char('\n');
    return result;
}

QString QnSendMailBusinessAction::stateString() const {
    QString result = QObject::tr("State: %1")
            .arg(getToggleState() == ToggleState::On
                 ? QObject::tr("On")
                 : QObject::tr("Off"));
    result += QLatin1Char('\n');
    return result;
}

QString QnSendMailBusinessAction::reasonString() const {
    BusinessEventType::Value eventType = QnBusinessEventRuntime::getEventType(m_runtimeParams);
    QnBusiness::EventReason reasonCode = QnBusinessEventRuntime::getReasonCode(m_runtimeParams);
    QString reasonText = QnBusinessEventRuntime::getReasonText(m_runtimeParams);

    QString result;
    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (eventType == BusinessEventType::Network_Issue)
                result = QObject::tr("No video frame received during last %1 seconds.")
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
                result = QObject::tr("Server terminated.");
            break;
        case QnBusiness::MServerIssueStarted:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QObject::tr("Server started after crash.");
            break;
        case QnBusiness::StorageIssueIoError:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QObject::tr("I/O Error occured at %1")
                                                  .arg(reasonText);
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QObject::tr("Not enough HDD/SSD speed for recording at %1.")
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
