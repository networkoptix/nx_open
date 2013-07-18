#include "business_strings_helper.h"
#include "version.h"

#include <api/app_server_connection.h>

#include <business/business_aggregation_info.h>
#include <business/events/reasoned_business_event.h>
#include <business/events/camera_input_business_event.h>
#include <business/events/conflict_business_event.h>

#include <core/resource/resource.h>
#include <core/resource/resource_name.h>
#include <core/resource_managment/resource_pool.h>

namespace {
    static const QString plainTextDelimiter(lit("\n"));
    static const QString htmlDelimiter(lit("<br>"));
}

QString QnBusinessStringsHelper::eventName(BusinessEventType::Value value) {

    if (value >= BusinessEventType::UserDefined)
        return tr("User Defined (%1)").arg((int)value - (int)BusinessEventType::UserDefined);

    switch( value )
    {
    case BusinessEventType::Camera_Motion:
        return tr("Motion on Camera");
    case BusinessEventType::Camera_Input:
        return tr("Input Signal on Camera");
    case BusinessEventType::Camera_Disconnect:
        return tr("Camera Disconnected");
    case BusinessEventType::Storage_Failure:
        return tr("Storage Failure");
    case BusinessEventType::Network_Issue:
        return tr("Network Issue");
    case BusinessEventType::Camera_Ip_Conflict:
        return tr("Camera IP Conflict");
    case BusinessEventType::MediaServer_Failure:
        return tr("Media Server Failure");
    case BusinessEventType::MediaServer_Conflict:
        return tr("Media Server Conflict");
    case BusinessEventType::AnyCameraIssue:
        return tr("Any camera issue");
    case BusinessEventType::AnyServerIssue:
        return tr("Any server issue");
    case BusinessEventType::AnyBusinessEvent:
        return tr("Any event");
    default:
        return QString();
    }
}

QString QnBusinessStringsHelper::eventAtResource(const QnBusinessEventParameters &params, bool useIp) {
    BusinessEventType::Value eventType = params.getEventType();
    QString resourceName = eventSource(params, useIp);
    switch (eventType) {
    case BusinessEventType::NotDefined:
        return tr("Undefined event has occured on %1").arg(resourceName);

    case BusinessEventType::Camera_Disconnect:
        return tr("Camera %1 was disconnected").arg(resourceName);

    case BusinessEventType::Camera_Input:
        return tr("Input on %1").arg(resourceName);

    case BusinessEventType::Camera_Motion:
        return tr("Motion on %1").arg(resourceName);

    case BusinessEventType::Storage_Failure:
        return tr("Storage Failure at %1").arg(resourceName);

    case BusinessEventType::Network_Issue:
        return tr("Network Issue at %1").arg(resourceName);

    case BusinessEventType::MediaServer_Failure:
        return tr("Media Server \"%1\" Failure").arg(resourceName);

    case BusinessEventType::Camera_Ip_Conflict:
        return tr("Camera IP Conflict at %1").arg(resourceName);

    case BusinessEventType::MediaServer_Conflict:
        return tr("Media Server \"%1\" Conflict").arg(resourceName);

    default:
        break;
    }
    return tr("Unknown Event has occured");
}


QString QnBusinessStringsHelper::eventDescription(const QnAbstractBusinessActionPtr& action,
                                                  const QnBusinessAggregationInfo &aggregationInfo,
                                                  bool useIp,
                                                  bool useHtml) {

    QString delimiter = useHtml
            ? htmlDelimiter
            : plainTextDelimiter;

    QnBusinessEventParameters params = action->getRuntimeParams();
    BusinessEventType::Value eventType = params.getEventType();

    QString result;
    result += tr("Event: %1").arg(eventName(eventType));

    result += delimiter;
    result += tr("Source: %1").arg(eventSource(params, useIp));

    if (useHtml && eventType == BusinessEventType::Camera_Motion) {
        result += delimiter;
        result += tr("Url: %1").arg(motionUrl(params));
    }

    result += delimiter;
    result += aggregatedEventDetails(action, aggregationInfo, delimiter);

    return result;
}

QString QnBusinessStringsHelper::eventDetails(const QnBusinessEventParameters &params, int aggregationCount, const QString &delimiter) {
    QString result;

    result += eventTimestamp(params, aggregationCount);

    BusinessEventType::Value eventType = params.getEventType();
    switch (eventType) {
    case BusinessEventType::Camera_Input: {
            result += delimiter;
            result = tr("Input port: %1").arg(params.getInputPortId());
            break;
        }
    case BusinessEventType::Storage_Failure:
    case BusinessEventType::Network_Issue:
    case BusinessEventType::MediaServer_Failure: {
            result += delimiter;
            result += tr("Reason: %1").arg(eventReason(params));
            break;
        }
    case BusinessEventType::Camera_Ip_Conflict: {
            result += delimiter;
            result += tr("Conflict address: %1").arg(params.getSource());

            int n = 0;
            foreach (QString mac, params.getConflicts()) {
                result += delimiter;
                result += tr("Camera #%1 MAC: %2").arg(++n).arg(mac);
            }

            break;
        }
    case BusinessEventType::MediaServer_Conflict: {
            int n = 0;
            foreach (QString ip, params.getConflicts()) {
                result += delimiter;
                result += tr("Conflicting EC #%1: %2").arg(++n).arg(ip);
            }
            break;
        }
    default:
        break;
    }
    return result;
}

QString QnBusinessStringsHelper::eventTimestamp(const QnBusinessEventParameters &params, int aggregationCount) {
    quint64 ts = params.getEventTimestamp();
    QDateTime time = QDateTime::fromMSecsSinceEpoch(ts/1000);

    int count = qMax(aggregationCount, 1);
    if (count == 1)
        return tr("Time: %1 on %2", "%1 means time, %2 means date")
            .arg(time.time().toString())
            .arg(time.date().toString());
    else
        return tr("First occurence: %1 on %2 (%n times total)", "%1 means time, %2 means date", count)
            .arg(time.time().toString())
            .arg(time.date().toString());
}

QString QnBusinessStringsHelper::eventSource(const QnBusinessEventParameters &params, bool useIp) {
    int id = params.getEventResourceId();
    QnResourcePtr res = id > 0 ? qnResPool->getResourceById(id, QnResourcePool::AllResources) : QnResourcePtr();
    return getFullResourceName(res, useIp);
}

QString QnBusinessStringsHelper::eventReason(const QnBusinessEventParameters& params) {
    QnBusiness::EventReason reasonCode = params.getReasonCode();
    BusinessEventType::Value eventType = params.getEventType();
    QString reasonText = params.getReasonText();

    QString result;

    switch (reasonCode) {
        case QnBusiness::NetworkIssueNoFrame:
            if (eventType == BusinessEventType::Network_Issue)
                result = QString(tr("No video frame received during last %1 seconds")).arg(reasonText);
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
                result = QString(tr("RTP packet loss detected, prev seq.=%1 next seq.=%2")).arg(seqs[0]).arg(seqs[1]);
            }
            break;
        case QnBusiness::MServerIssueTerminated:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QString(tr("Server terminated"));
            break;
        case QnBusiness::MServerIssueStarted:
            if (eventType == BusinessEventType::MediaServer_Failure)
                result = QString(tr("Server started after crash"));
            break;
        case QnBusiness::StorageIssueIoError:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QString(tr("I/O Error occured at %1").arg(reasonText));
            break;
        case QnBusiness::StorageIssueNotEnoughSpeed:
            if (eventType == BusinessEventType::Storage_Failure)
                result = QString(tr("Not enough HDD/SSD speed for recording to %1").arg(reasonText));
            break;
        default:
            break;
    }

    return result;
}

QString QnBusinessStringsHelper::aggregatedEventDetails(const QnAbstractBusinessActionPtr& action,
                                              const QnBusinessAggregationInfo& aggregationInfo,
                                              const QString& delimiter) {
    QString result;
    if (aggregationInfo.isEmpty()) {
        result += eventDetails(action->getRuntimeParams(), action->getAggregationCount(), delimiter);
    }

    foreach (QnInfoDetail detail, aggregationInfo.toList()) {
        if (!result.isEmpty())
            result += delimiter;
        result += eventDetails(detail.runtimeParams, detail.count, delimiter);
    }
    return result;
}

QString QnBusinessStringsHelper::motionUrl(const QnBusinessEventParameters &params) {
    int id = params.getEventResourceId();
    QnNetworkResourcePtr res = id > 0 ? 
                            qnResPool->getResourceById(id, QnResourcePool::AllResources).dynamicCast<QnNetworkResource>() : 
                            QnNetworkResourcePtr();
    if (!res)
        return QString();

    QnResourcePtr mserverRes = res->getParentResource();
    if (!mserverRes)
        return QString();

    QUrl apPServerUrl = QnAppServerConnectionFactory::publicUrl();
    QUrl appServerDefaultUrl = QnAppServerConnectionFactory::defaultUrl();
    quint64 ts = params.getEventTimestamp();

    QnCameraHistoryPtr history = QnCameraHistoryPool::instance()->getCameraHistory(res->getPhysicalId());
    if (history) {
        QnTimePeriod period;
        QnMediaServerResourcePtr newServer = history->getMediaServerOnTime(ts/1000, true, period, false);
        if (newServer)
            mserverRes = newServer;
    }

    QUrl mserverUrl = mserverRes->getUrl();
    if (resolveAddress(apPServerUrl.host()) == QHostAddress::LocalHost) {
        if (resolveAddress(appServerDefaultUrl.host()) != QHostAddress::LocalHost) {
            apPServerUrl = appServerDefaultUrl;
        } else {
            apPServerUrl.setHost(mserverUrl.host());
        }
    }

    QString result(lit("https://%1:%2/web/camera?physical_id=%3&pos=%4"));
    result = result.arg(apPServerUrl.host()).arg(apPServerUrl.port(80)).arg(res->getPhysicalId()).arg(ts/1000);

    return result;
}

