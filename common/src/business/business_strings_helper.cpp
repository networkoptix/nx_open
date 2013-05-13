#include "business_strings_helper.h"
#include "events/reasoned_business_event.h"
#include "events/camera_input_business_event.h"
#include "business_aggregation_info.h"
#include "version.h"
#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"
#include "api/app_server_connection.h"
#include "events/conflict_business_event.h"

QString QnBusinessStringsHelper::reasonString(const QnBusinessEventParameters &params) 
{
    BusinessEventType::Value eventType = params.getEventType();
    QnBusiness::EventReason reasonCode = params.getReasonCode();
    QString reasonText = params.getReasonText();

    QString result;
    switch (reasonCode) {
    case QnBusiness::NetworkIssueNoFrame:
        if (eventType == BusinessEventType::Network_Issue)
            result = QObject::tr("No video frames were received during the last %1 seconds")
            .arg(reasonText);
        break;
    case QnBusiness::NetworkIssueConnectionClosed:
        if (eventType == BusinessEventType::Network_Issue)
            result = QObject::tr("Connection to camera was closed unexpectedly");
        break;
    case QnBusiness::NetworkIssueRtpPacketLoss:
        if (eventType == BusinessEventType::Network_Issue) {
            result = QObject::tr("RTP packet was loss detected");
        }
        break;
    case QnBusiness::MServerIssueTerminated:
        if (eventType == BusinessEventType::MediaServer_Failure)
            result = QObject::tr("Server has been terminated");
        break;
    case QnBusiness::MServerIssueStarted:
        if (eventType == BusinessEventType::MediaServer_Failure)
            result = QObject::tr("Server started after crash");
        break;
    case QnBusiness::StorageIssueIoError:
        if (eventType == BusinessEventType::Storage_Failure)
            result = QObject::tr("An error has occured while writing to %1")
            .arg(reasonText);
        break;
    case QnBusiness::StorageIssueNotEnoughSpeed:
        if (eventType == BusinessEventType::Storage_Failure)
            result = QObject::tr("Writing speed of HDD/SSD at %1 was found to be insufficient to sustain continuous recording")
            .arg(reasonText);
        break;
    default:
        break;
    }

    if (!result.isEmpty())
        result += QLatin1Char('\n');
    return result;

}

QString QnBusinessStringsHelper::timestampString(const QnBusinessEventParameters &params, int aggregationCount) 
{
    quint64 ts = params.getEventTimestamp();

    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    QString result;
    int count = qMax(aggregationCount, 1);
    if (count == 1)
        result = QObject::tr("at %1 on %2", "%1 means time, %2 means date")
        .arg(time.time().toString())
        .arg(time.date().toString());
    else
        result = QObject::tr("%n times since %1 %2", "%1 means time, %2 means date", count)
        .arg(time.time().toString())
        .arg(time.date().toString());
    result += QLatin1Char('\n');
    return result;
}

QString QnBusinessStringsHelper::resourceUrl(const QnBusinessEventParameters &params) 
{
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    return res ? QString(QLatin1String("%1 (%2)")).arg(res->getName()).arg(res->getUrl()) : QString();
}

QString QnBusinessStringsHelper::resourceName(const QnBusinessEventParameters &params) 
{
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    return res ? res->getName() : QString();
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params)
{
    QnBusiness::EventReason reasonCode = params.getReasonCode();
    BusinessEventType::Value eventType = params.getEventType();
    QString reasonText = params.getReasonText();

    QString result;

    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (eventType == BusinessEventType::Network_Issue)
                result = QString(tr("No video frame received\nduring last %1 seconds.")).arg(reasonText);
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            if (eventType == BusinessEventType::Network_Issue)
                result = QString(tr("Connection to camera\nwas unexpectedly closed"));
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            if (eventType == BusinessEventType::Network_Issue) {
                QStringList seqs = reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                result = QString(tr("RTP packet loss detected.\nPrev seq.=%1 next seq.=%2")).arg(seqs[0]).arg(seqs[1]);
            }
            break;
        case QnBusiness::MServerIssueTerminated:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QString(tr("Server terminated."));
            break;
        case QnBusiness::MServerIssueStarted:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QString(tr("Server started after crash."));
            break;
        case QnBusiness::StorageIssueIoError:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QString(tr("I/O Error occured at\n%1").arg(reasonText));
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QString(tr("Not enough HDD/SSD speed\nfor recording at\n%1.").arg(reasonText));
            break;
        default:
            break;
    }

    return result;
}

QString QnBusinessStringsHelper::longEventDescription(const QnAbstractBusinessAction* action, const QnBusinessAggregationInfo& aggregationInfo)
{
    BusinessEventType::Value eventType = action->getRuntimeParams().getEventType();
    QString resourceName = resourceUrl(action->getRuntimeParams());
    QString serverName = QObject::tr("%1 Server").arg(QLatin1String(VER_COMPANYNAME_STR));
    int issueCount = qMax(aggregationInfo.totalCount(), 1);

    QString txt;
    QString messageBody;

    switch (eventType) {
    case BusinessEventType::NotDefined:
        messageBody = tr("Undefined event has occured");
        break;

    case BusinessEventType::Camera_Disconnect:
        messageBody = tr("%1 has detected that camera %2 was disconnected")
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::Camera_Input:
        messageBody = tr("%1 has caught an input signal on camera %2:")
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::Camera_Motion:

        txt = QObject::tr("%1 has detected motion on camera %2")
            .arg(serverName)
            .arg(resourceName);
        messageBody = QString(lit("<a href=\"%1\">%2</a>")).arg(motionUrl(action->getRuntimeParams())).arg(txt);
        break;
    case BusinessEventType::Storage_Failure:
        messageBody = tr("%1 \"%2\" has detected %n storage issues:", "", issueCount)
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::Network_Issue:
        messageBody = tr("%1 has experienced %n network issues with camera %2:", "", issueCount)
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::MediaServer_Failure:
        messageBody = tr("%1 \"%2\" failure was detected:")
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::Camera_Ip_Conflict:
        messageBody = tr("%1 \"%2\" has detected camera IP conflict:")
            .arg(serverName)
            .arg(resourceName);
        break;
    case BusinessEventType::MediaServer_Conflict:
        messageBody = tr("%1 \"%2\" is conflicting with other server:")
            .arg(serverName)
            .arg(resourceName);
        break;
    default:
        if (eventType >= BusinessEventType::UserDefined)
            messageBody = tr("User Defined Event (%1) has occured on %2")
            .arg((int)eventType - (int)BusinessEventType::UserDefined)
            .arg(serverName);
        else
            messageBody = tr("Unknown Event has occured on").arg(serverName);
        break;
    }
    messageBody += lit("<br>");

    if (aggregationInfo.totalCount() == 0) {
        messageBody += eventTextString(eventType, action->getRuntimeParams());
        messageBody += timestampString(action->getRuntimeParams(), action->getAggregationCount());
    }
    foreach (QnInfoDetail detail, aggregationInfo.toList()) {
        messageBody += eventTextString(eventType, detail.runtimeParams);
        messageBody += timestampString(detail.runtimeParams, detail.count);
    }

    return messageBody;
}

QString QnBusinessStringsHelper::motionUrl(const QnBusinessEventParameters &params)
{
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    if (!res)
        return QString();
    QnResourcePtr mserverRes = res->getParentResource();
    if (!mserverRes)
        return QString();

    QUrl apPServerUrl = QnAppServerConnectionFactory::defaultUrl();
    QUrl mserverUrl = mserverRes->getUrl();
    quint64 ts = params.getEventTimestamp();
    QByteArray rnd = QByteArray::number(rand()).toHex();

    QString result(lit("https://%1:%2/proxy/http/%3:%4/media/%5.webm?rand=%6&resolution=240p&pos=%7"));
    result = result.arg(apPServerUrl.host()).arg(apPServerUrl.port(80)).arg(mserverUrl.host()).arg(mserverUrl.port(80)).
        arg(res->getUniqueId()).arg(QLatin1String(rnd)).arg(ts/1000);

    return result;
}

QString QnBusinessStringsHelper::conflictString(const QnBusinessEventParameters &params)
{
    QString source = params.getSource();
    QStringList conflicts = params.getConflicts();

    QString result = source;
    result += QObject::tr("conflicted with:");
    result += QLatin1Char('\n');
    foreach(const QString& entity, conflicts)
        result += entity + QLatin1Char('\n');
    return result;
}

QString QnBusinessStringsHelper::eventTextString(BusinessEventType::Value eventType, const QnBusinessEventParameters &params) 
{
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
                .arg(params.getInputPortId());
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
        result += QLatin1Char('\n');
        result += conflictString(params);
        break;
    default:
        break;
    }
    return result;
}
