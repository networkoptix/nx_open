#include "VideoSessionManager.h"
#include <QDebug>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlInputSource>
#include <QSharedPointer>
#include "QStdIStream.h"
#include "AppSessionManager.h"

namespace {
    const unsigned long XSD_FLAGS = xml_schema::flags::dont_initialize | xml_schema::flags::dont_validate;

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

} // anonymous namespace


void detail::VideoServerSessionManagerReplyProcessor::at_replyReceived(int status, const QByteArray &reply, int handle)
{
    QnApiRecordedTimePeriodsResponsePtr result;
    if(status == 0)
        status = parseRecordedTimePeriods(reply, &result);

    emit finished(status, result, handle);

    deleteLater();
}

VideoServerSessionManager::VideoServerSessionManager(const QUrl &url, QObject *parent)
    : SessionManager(url, parent)
{
}

int VideoServerSessionManager::recordedTimePeriods(const QnRequestParamList& params, QnApiRecordedTimePeriodsResponsePtr& timePeriodList)
{
    QByteArray reply;
    if(sendGetRequest("api/RecordedTimePeriods", params, reply) != 0)
        return 1;

    return parseRecordedTimePeriods(reply, &timePeriodList);
}

int VideoServerSessionManager::asyncRecordedTimePeriods(const QnRequestParamList& params, QObject *target, const char *slot)
{
    detail::VideoServerSessionManagerReplyProcessor *processor = new detail::VideoServerSessionManagerReplyProcessor();
    connect(processor, SIGNAL(finished(int, const QnApiRecordedTimePeriodsResponsePtr &, int)), target, slot);
    return sendAsyncGetRequest("api/RecordedTimePeriods", params, processor, SLOT(at_replyReceived(int, const QByteArray &, int)));
}

