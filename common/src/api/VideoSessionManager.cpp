#include "VideoSessionManager.h"
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "AppSessionManager.h"
#include "utils/common/util.h"

namespace {
    const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;

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
            buffer.read(((char*) &startTimeMs) + 2, 6);
            buffer.read(((char*) &durationMs) + 2, 6);
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

void detail::VideoServerSessionManagerReplyProcessor::at_replyReceived(int status, const QByteArray &reply, int handle)
{
    QnTimePeriodList result;
    if(status == 0) 
    {
        if (reply.startsWith("BIN"))
        {
            result = parseBinaryTimePeriods(reply);
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

VideoServerSessionManager::VideoServerSessionManager(const QUrl &url, QObject *parent)
    : SessionManager(url, parent)
{
}

int VideoServerSessionManager::recordedTimePeriods(const QnRequestParamList& params, QnTimePeriodList& result)
{
    QByteArray reply;
    if(sendGetRequest("api/RecordedTimePeriods", params, reply) != 0)
        return 1;
    QnApiRecordedTimePeriodsResponsePtr timePeriodList;
    int status = parseRecordedTimePeriods(reply, &timePeriodList);
    if (status == 0)
        result = parseRecordedTimePeriods(timePeriodList);
    return status;
}

int VideoServerSessionManager::asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerReplyProcessor *processor = new detail::VideoServerSessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnTimePeriodList&, int)), target, slot);
    return sendAsyncGetRequest("api/RecordedTimePeriods", params, processor, SLOT(at_replyReceived(int, const QByteArray &, int)));
}

