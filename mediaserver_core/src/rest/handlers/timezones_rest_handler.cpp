#include <QFile>
#include <QtCore/QProcess>
#include <QtCore/QTimeZone>

#include <nx/fusion/model_functions.h>
#include <nx/utils/time.h>

#include "timezones_rest_handler.h"

namespace {

struct TimeZoneInfo
{
    TimeZoneInfo():
        id(),
        offsetFromUtc(0),
        displayName(),
        hasDaylightTime(false),
        isDaylightTime(false),
        comment()
    {
    }

    QString id;
    int offsetFromUtc;
    QString displayName;
    bool hasDaylightTime;
    bool isDaylightTime;
    QString comment;
};

#define TimeZoneInfo_Fields \
    (id)(offsetFromUtc)(displayName)(hasDaylightTime)(isDaylightTime)(comment)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES((TimeZoneInfo), (json), _Fields)

} // namespace

int QnGetTimeZonesRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& /*params*/,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* /*owner*/)
{
    std::vector<TimeZoneInfo> outputData;

    for (const auto& timeZoneId: QTimeZone::availableTimeZoneIds())
    {
        if (nx::utils::getTimeZoneFile(timeZoneId).isNull())
            continue;

        QTimeZone timeZone(timeZoneId);
        TimeZoneInfo record;
        record.id = timeZone.id();
        record.displayName = timeZone.displayName(
            QDateTime::currentDateTime(), QTimeZone::DefaultName);
        record.comment = timeZone.comment();
        record.hasDaylightTime = timeZone.hasDaylightTime();
        record.isDaylightTime = timeZone.isDaylightTime(QDateTime::currentDateTime());
        record.offsetFromUtc = timeZone.offsetFromUtc(QDateTime::currentDateTime());
        outputData.emplace_back(std::move(record));
    }

    result.setReply(outputData);

    return nx_http::StatusCode::ok;
}
