#include "VideoServerConnection.h"
#include "xsd_RecordedTimePeriods.h"
#include "VideoSessionManager.h"

QnVideoServerConnection::QnVideoServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth):
    m_sessionManager( new VideoServerSessionManager(host, port, auth))
{
    m_sessionManager->setAddEndShash(false);
}

QnTimePeriodList QnVideoServerConnection::recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail)
{
    using namespace xsd::api::recordedTimePeriods;

    QnTimePeriodList result;
    QnApiRecordedTimePeriodsResponsePtr timePeriodList;
    QnRequestParamList params;

    foreach(QnNetworkResourcePtr netResource, list) {
        params << QnRequestParam("mac", netResource->getMAC().toString());
    }
    params << QnRequestParam("startTime", QString::number(startTimeUSec));
    params << QnRequestParam("endTime", QString::number(endTimeUSec));
    params << QnRequestParam("detail", QString::number(detail));

    if (m_sessionManager->recordedTimePeriods(params, timePeriodList) == 0)
    {
        for (RecordedTimePeriods::timePeriod_const_iterator i = timePeriodList->timePeriod().begin(); i != timePeriodList->timePeriod().end(); ++i)
            result << QnTimePeriod(i->startTime(), i->duration());
    }
    return result;
}
