#include "VideoServerConnection.h"
#include "xsd_RecordedTimePeriods.h"
#include "VideoSessionManager.h"

QnVideoServerConnection::QnVideoServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth):
    m_sessionManager( new VideoServerSessionManager(host, port, auth))
{

}

QnTimePeriodList QnVideoServerConnection::recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTime, qint64 endTime, qint64 detail)
{
    using namespace xsd::api::recordedTimePeriods;

    QnTimePeriodList result;
    QnApiRecordedTimePeriodsResponsePtr timePeriodList;
    QnRequestParamList params;

    foreach(QnNetworkResourcePtr netResource, list) {
        params << QnRequestParam("mac", netResource->getMAC().toString());
    }
    params << QnRequestParam("startTime", QString::number(startTime));
    params << QnRequestParam("endTime", QString::number(endTime));
    params << QnRequestParam("detail", QString::number(detail));

    m_sessionManager->recordedTimePeriods(params, timePeriodList);

    for (RecordedTimePeriods::timePeriod_const_iterator i = timePeriodList->timePeriod().begin(); i != timePeriodList->timePeriod().end(); ++i)
    {
        result << QnTimePeriod(i->startTime(), i->duration());
    }

    return result;
}
