
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "utils/common/util.h"

#include "xsd_recordedTimePeriods.h"

#include "VideoServerConnection.h"
#include "VideoServerConnection_p.h"
#include "xsd_recordedTimePeriods.h"
#include "SessionManager.h"

namespace {
    const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;

    // for video server methods
    typedef QSharedPointer<xsd::api::recordedTimePeriods::RecordedTimePeriods>           QnApiRecordedTimePeriodsResponsePtr;

    QnTimePeriodList parseBinaryTimePeriods(const QByteArray &reply)
    {
        QnTimePeriodList result;

        QByteArray localReply = reply;
        QBuffer buffer(&localReply);
        buffer.open(QIODevice::ReadOnly);

        QStdIStream is(&buffer);

        char format[3];
        buffer.read(format, 3);
        if (QByteArray(format, 3) != "BIN")
            return result;
        while (buffer.size() - buffer.pos() >= 12)
        {
            qint64 startTimeMs = 0;
            qint64 durationMs = 0;
            buffer.read(((char*) &startTimeMs), 6);
            buffer.read(((char*) &durationMs), 6);
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
            startTimeMs <<= 16;
            durationMs <<= 16;
#else
            startTimeMs >>= 16;
            durationMs >>= 16;
#endif
            result << QnTimePeriod(ntohll(startTimeMs), ntohll(durationMs));
        }

        return result;
    }

    int parseRecordedTimePeriods(const QByteArray &reply, QnApiRecordedTimePeriodsResponsePtr *result)
    {
        using xsd::api::recordedTimePeriods::recordedTimePeriods;

        try {
            QByteArray localReply = reply;
            QBuffer buffer(&localReply);
            buffer.open(QIODevice::ReadOnly);

            QStdIStream is(&buffer);

            *result = QnApiRecordedTimePeriodsResponsePtr(recordedTimePeriods(is, XSD_FLAGS).release());

            return 0;
        } catch (const xml_schema::exception& e) {
            qDebug(e.what());

            return 1;
        }
    }

    QnTimePeriodList parseRecordedTimePeriods(const QnApiRecordedTimePeriodsResponsePtr &reply) {
        using namespace xsd::api::recordedTimePeriods;

        QnTimePeriodList result;
        for (RecordedTimePeriods::timePeriod_const_iterator i = reply->timePeriod().begin(); i != reply->timePeriod().end(); ++i)
            result << QnTimePeriod(i->startTime(), i->duration());

        return result;
    }
} // anonymous namespace

void detail::QnVideoServerConnectionReplyProcessor::at_replyReceived(int status, const QnTimePeriodList &result, int handle)
{
    emit finished(status, result, handle);
    deleteLater();
}

QnVideoServerConnection::QnVideoServerConnection(const QUrl &url, QObject *parent):
    QObject(parent),
    m_url(url)
{
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
    QByteArray errorString;
    int status = recordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), result, errorString);
    if (status)
    {
        qDebug() << errorString;
    }

    return result;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnNetworkResourceList& list, qint64 startTimeMs, qint64 endTimeMs, qint64 detail, QRegion motionRegion, QObject *target, const char *slot) {
    detail::QnVideoServerConnectionReplyProcessor *processor = new detail::QnVideoServerConnectionReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList &, int)), target, slot);

    return asyncRecordedTimePeriods(createParamList(list, startTimeMs, endTimeMs, detail, motionRegion), processor, SLOT(at_replyReceived(int, const QnTimePeriodList&, int)));
}

void detail::VideoServerSessionManagerReplyProcessor::at_replyReceived(int status, const QByteArray &reply, const QByteArray& errorString, int handle)
{
    QnTimePeriodList result;
    if(status == 0)
    {
        if (reply.startsWith("BIN"))
        {
            QnTimePeriod::decode((const quint8*) reply.constData()+3, reply.size()-3, result);
        }
        else {
            QnApiRecordedTimePeriodsResponsePtr xmlResponse;
            status = parseRecordedTimePeriods(reply, &xmlResponse);
            if (status == 0)
                result = parseRecordedTimePeriods(xmlResponse);
        }
    }

    emit finished(status, result, handle);

    deleteLater();
}

int QnVideoServerConnection::recordedTimePeriods(const QnRequestParamList& params, QnTimePeriodList& result, QByteArray& errorString)
{
    QByteArray reply;

    if(SessionManager::instance()->sendGetRequest(m_url, "RecordedTimePeriods", params, reply, errorString))
        return 1;

    if (reply.startsWith("BIN"))
    {
        QnTimePeriod::decode((const quint8*) reply.constData()+3, reply.size()-3, result);
    } else {
        QnApiRecordedTimePeriodsResponsePtr timePeriodList;
        int status = parseRecordedTimePeriods(reply, &timePeriodList);
        if (status == 0)
            result = parseRecordedTimePeriods(timePeriodList);
        return status;
    }

    return 0;
}

int QnVideoServerConnection::asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerReplyProcessor *processor = new detail::VideoServerSessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList&, int)), target, slot);

    return SessionManager::instance()->sendAsyncGetRequest(m_url, "RecordedTimePeriods", params, processor, SLOT(at_replyReceived(int, QByteArray, QByteArray, int)));
}

