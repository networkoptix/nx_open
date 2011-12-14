#include "VideoServerConnection.h"
#include "VideoServerConnection_p.h"
#include "xsd_RecordedTimePeriods.h"
#include "VideoSessionManager.h"

namespace {
    QnTimePeriodList parseRecordedTimePeriods(const QnApiRecordedTimePeriodsResponsePtr &reply) {
        using namespace xsd::api::recordedTimePeriods;

        QnTimePeriodList result;
        for (RecordedTimePeriods::timePeriod_const_iterator i = reply->timePeriod().begin(); i != reply->timePeriod().end(); ++i)
            result << QnTimePeriod(i->startTime(), i->duration());

        return result;
    }

} // anonymous namespace

void detail::QnVideoServerConnectionReplyProcessor::at_replyReceived(int status, const QnApiRecordedTimePeriodsResponsePtr &reply) 
{
    QnTimePeriodList result;
    if(status == 0)
        result = parseRecordedTimePeriods(reply);

    emit finished(result);
    
    deleteLater();
}


QnVideoServerConnection::QnVideoServerConnection(const QHostAddress& host, int port, const QAuthenticator& auth, QObject *parent):
    QObject(parent),
    m_sessionManager(new VideoServerSessionManager(host, port, auth))
{
    m_sessionManager->setAddEndShash(false);
}

QnVideoServerConnection::~QnVideoServerConnection() {}

QnRequestParamList QnVideoServerConnection::createParamList(const QnNetworkResourceList& list, qint64 startTimeUSec, qint64 endTimeUSec, qint64 detail, const QRegion& motionRegion) 
{
    QnRequestParamList result;

    foreach(QnNetworkResourcePtr netResource, list)
        result << QnRequestParam("mac", netResource->getMAC().toString());
    result << QnRequestParam("startTime", QString::number(startTimeUSec));
    result << QnRequestParam("endTime", QString::number(endTimeUSec));
    result << QnRequestParam("detail", QString::number(detail));
    for (int i = 0; i < motionRegion.rects().size(); ++i)
    {
        QRect r = motionRegion.rects().at(i);
        QString rectStr;
        QTextStream str(&rectStr);
        str << r.left() << ',' << r.top() << ',' << r.width() << ',' << r.height();
        str.flush();
        result << QnRequestParam("motionRect", rectStr);
    }

    return result;
}

QnTimePeriodList QnVideoServerConnection::recordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, const QRegion& motionRegion)
{
    QnTimePeriodList result;
    QnApiRecordedTimePeriodsResponsePtr timePeriodList;
    if (m_sessionManager->recordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), timePeriodList) == 0)
        result = parseRecordedTimePeriods(timePeriodList);

    return result;
}

void QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QRegion motionRegion, QObject *target, const char *slot) {
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(const QnTimePeriodList &)), target, slot);

    m_sessionManager->asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), processor, SLOT(at_replyReceived(int, const QnApiRecordedTimePeriodsResponsePtr &)));
}