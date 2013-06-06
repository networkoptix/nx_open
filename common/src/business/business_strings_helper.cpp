#include "business_strings_helper.h"
#include "events/reasoned_business_event.h"
#include "events/camera_input_business_event.h"
#include "business_aggregation_info.h"
#include "version.h"
#include "core/resource/resource.h"
#include "core/resource_managment/resource_pool.h"
#include "api/app_server_connection.h"
#include "events/conflict_business_event.h"

QString QnBusinessStringsHelper::eventName(BusinessEventType::Value value) {
    return BusinessEventType::toString(value); //TODO: #GDM refactor, toString should not be used in public
}

QString QnBusinessStringsHelper::shortEventDescription(const QnBusinessEventParameters &params) {

    BusinessEventType::Value eventType = params.getEventType();
    QString resourceName = resourceUrl(params);
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

QString QnBusinessStringsHelper::eventDescription(const QnAbstractBusinessActionPtr &action,
                                                  const QnBusinessAggregationInfo& aggregationInfo) {
    BusinessEventType::Value eventType = action->getRuntimeParams().getEventType();
    QString resourceName = resourceUrl(action->getRuntimeParams());
    QString serverName = QObject::tr("%1 Server").arg(QLatin1String(VER_COMPANYNAME_STR));
    int issueCount = qMax(aggregationInfo.totalCount(), 1);

    switch (eventType) {
    case BusinessEventType::NotDefined:
        return tr("Undefined event has occured");
    case BusinessEventType::Camera_Disconnect:
        return tr("%1 has detected that camera %2 was disconnected")
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::Camera_Input:
        return tr("%1 has caught an input signal on camera %2")
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::Camera_Motion:
        return QObject::tr("%1 has detected motion on camera %2")
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::Storage_Failure:
        return tr("%1 \"%2\" has detected %n storage issues", "", issueCount)
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::Network_Issue:
        return tr("%1 has experienced %n network issues with camera %2", "", issueCount)
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::MediaServer_Failure:
        return tr("%1 \"%2\" failure was detected")
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::Camera_Ip_Conflict:
        return tr("%1 \"%2\" has detected camera IP conflict")
            .arg(serverName)
            .arg(resourceName);
    case BusinessEventType::MediaServer_Conflict:
        return tr("%1 \"%2\" is conflicting with other server")
            .arg(serverName)
            .arg(resourceName);
    default:
        if (eventType >= BusinessEventType::UserDefined)
            return tr("User Defined Event (%1) has occured on %2")
                    .arg((int)eventType - (int)BusinessEventType::UserDefined)
                    .arg(serverName);
        else
            return tr("Unknown Event has occured on %1").arg(serverName);
    }
    return QString();
}

QString QnBusinessStringsHelper::timestampString(const QnBusinessEventParameters &params, int aggregationCount) {
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
    return result;
}

QString QnBusinessStringsHelper::resourceUrl(const QnBusinessEventParameters &params) {
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    return res ? QString(QLatin1String("%1 (%2)")).arg(res->getName()).arg(res->getUrl()) : QString();
}

QString QnBusinessStringsHelper::resourceName(const QnBusinessEventParameters &params) {
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::rfAllResources) : QnResourcePtr();
    return res ? res->getName() : QString();
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) {
    QnBusiness::EventReason reasonCode = params.getReasonCode();
    BusinessEventType::Value eventType = params.getEventType();
    QString reasonText = params.getReasonText();

    QString result;

    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (eventType == BusinessEventType::Network_Issue)
                result = QString(tr("No video frame received during last %1 seconds.")).arg(reasonText);
            break;
        case QnBusiness::NetworkIssueConnectionClosed:
            if (eventType == BusinessEventType::Network_Issue)
                result = QString(tr("Connection to camera was unexpectedly closed"));
            break;
        case QnBusiness::NetworkIssueRtpPacketLoss:
            if (eventType == BusinessEventType::Network_Issue) {
                QStringList seqs = reasonText.split(QLatin1Char(';'));
                if (seqs.size() != 2)
                    break;
                result = QString(tr("RTP packet loss detected. Prev seq.=%1 next seq.=%2")).arg(seqs[0]).arg(seqs[1]);
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
                result = QString(tr("I/O Error occured at %1").arg(reasonText));
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QString(tr("Not enough HDD/SSD speed\nfor recording to %1.").arg(reasonText));
            break;
        default:
            break;
    }

    return result;
}


QString QnBusinessStringsHelper::longEventDescription(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo& aggregationInfo) {
    static const QString DELIM(lit("\n"));

    QString result = eventDescription(action, aggregationInfo);
    result += DELIM;
    result += eventDetails(action, aggregationInfo, DELIM);
    return result;
}

QString QnBusinessStringsHelper::longEventDescriptionHtml(const QnAbstractBusinessActionPtr& action, const QnBusinessAggregationInfo& aggregationInfo) {
    static const QString DELIM(lit("<br>"));

    QString result = eventDescription(action, aggregationInfo);

    BusinessEventType::Value eventType = action->getRuntimeParams().getEventType();
    if (eventType == BusinessEventType::Camera_Motion)
        result = QString(lit("<a href=\"%1\">%2</a>")).arg(motionUrl(action->getRuntimeParams())).arg(result);
    result += DELIM;
    result += eventDetails(action, aggregationInfo, DELIM);
    return result;
}

QString QnBusinessStringsHelper::eventDetails(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo,
                                              const QString& delimiter) {
    BusinessEventType::Value eventType = action->getRuntimeParams().getEventType();
    QString result;

    int count = 0;
    if (aggregationInfo.isEmpty()) {
        QString params = eventParamsString(eventType, action->getRuntimeParams());
        result += params;
        if (!params.isEmpty())
            result += delimiter;
        result += timestampString(action->getRuntimeParams(), action->getAggregationCount());
        count++;
    }
    foreach (QnInfoDetail detail, aggregationInfo.toList()) {
        if (count > 0)
            result += delimiter;
        QString params = eventParamsString(eventType, detail.runtimeParams);
        result += params;
        if (!params.isEmpty())
            result += delimiter;
        result += timestampString(detail.runtimeParams, detail.count);
        count++;
    }
    return result;
}

QString QnBusinessStringsHelper::motionUrl(const QnBusinessEventParameters &params) {
    int id = params.getEventResourceId();
    QnNetworkResourcePtr res = id > 0 ? 
                            qnResPool->getResourceById(id, QnResourcePool::rfAllResources).dynamicCast<QnNetworkResource>() : 
                            QnNetworkResourcePtr();
    if (!res)
        return QString();

    QnResourcePtr mserverRes = res->getParentResource();
    if (!mserverRes)
        return QString();

    QUrl apPServerUrl = QnAppServerConnectionFactory::defaultUrl();
    quint64 ts = params.getEventTimestamp();
    QByteArray rnd = QByteArray::number(qrand()).toHex();

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(res->getPhysicalId());
    if (history) {
        QnTimePeriod period;
        QnMediaServerResourcePtr newServer = history->getMediaServerOnTime(ts/1000, true, period, false);
        if (newServer)
            mserverRes = newServer;
    }
    QUrl mserverUrl = mserverRes->getUrl();


    QString result(lit("https://%1:%2/proxy/http/%3:%4/media/%5.webm?rand=%6&resolution=240p&pos=%7"));
    result = result.arg(apPServerUrl.host()).arg(apPServerUrl.port(80)).arg(mserverUrl.host()).arg(mserverUrl.port(80)).
        arg(res->getPhysicalId()).arg(QLatin1String(rnd)).arg(ts/1000);

    return result;
}

QString QnBusinessStringsHelper::conflictString(const QnBusinessEventParameters &params) {
    QStringList conflicts = params.getConflicts();

    QString result = QObject::tr("%1 conflicted with: ").arg(params.getSource());
    for (int i = 0; i < conflicts.size(); ++i) {
        if (i > 0)
            result += lit(", ");
        result += conflicts[i];
    }
    return result;
}

QString QnBusinessStringsHelper::eventParamsString(BusinessEventType::Value eventType, const QnBusinessEventParameters &params) {
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
        result = QObject::tr("Input port: %1").arg(params.getInputPortId());
        break;
    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::MediaServer_Failure:
        result = eventReason(params);
        break;
    case BusinessEventType::Camera_Ip_Conflict:
    case BusinessEventType::MediaServer_Conflict:
        result = conflictString(params);
        break;
    default:
        break;
    }
    return result;
}

QString QnBusinessStringsHelper::formatEmailList(const QStringList& value) {
    QString result;
    for (int i = 0; i < value.size(); ++i)
    {
        if (i > 0)
            result.append(L';');
        result.append(QString(QLatin1String("<%1>")).arg(value[i]));
    }
    return result;
}
