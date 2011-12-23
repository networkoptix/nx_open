#include "VideoServerConnection.h"
#include "VideoServerConnection_p.h"
#include "xsd_RecordedTimePeriods.h"
#include "VideoSessionManager.h"

void detail::QnVideoServerConnectionReplyProcessor::at_replyReceived(int status, const QnTimePeriodList &result, int handle)
{
    emit finished(status, result, handle);
    deleteLater();
}

QnVideoServerConnection::QnVideoServerConnection(const QUrl &url, QObject *parent):
    QObject(parent),
    m_sessionManager(new VideoServerSessionManager(url))
{
    m_sessionManager->setAddEndSlash(false);
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
    result << QnRequestParam("format", "bin");
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
    int status = m_sessionManager->recordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), result);
    return result;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QRegion motionRegion, QObject *target, const char *slot) {
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return m_sessionManager->asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}