#include "business_event_parameters.h"

#include <QtCore/QVariant>

#include "events/abstract_business_event.h"

#include <utils/common/id.h>
#include <utils/common/model_functions.h>

static const char DELIMITER('$');
static const char STRING_LIST_DELIM('\n');

QnBusinessEventParameters::QnBusinessEventParameters():
    eventType(QnBusiness::UndefinedEvent),
    eventTimestamp(0),
    reasonCode(QnBusiness::NoReason)
{}

inline int toInt(const QByteArray& ba)
{
    const char* curPtr = ba.data();
    const char* end = curPtr + ba.size();
    int result = 0;
    for(; curPtr < end; ++curPtr)
    {
        if (*curPtr < '0' || *curPtr > '9')
            return result;
        result = result*10 + (*curPtr - '0');
    }
    return result;
}

inline qint64 toInt64(const QByteArray& ba)
{
    const char* curPtr = ba.data();
    const char* end = curPtr + ba.size();
    qint64 result = 0ll;
    for(; curPtr < end; ++curPtr)
    {
        if (*curPtr < '0' || *curPtr > '9')
            return result;
        result = result*10 + (*curPtr - '0');
    }
    return result;
}

QnBusinessEventParameters QnBusinessEventParameters::unpack(const QByteArray& value)
{
    QnBusinessEventParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < ParamCount)
    {
        int nextPos = value.indexOf(DELIMITER, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field = QByteArray::fromRawData(value.data() + prevPos + 1, nextPos - prevPos - 1);
        if (!field.isEmpty())
        {
            switch ((Param) i)
            {
                case EventTypeParam:
                    result.eventType = (QnBusiness::EventType) toInt(field);
                    break;
                case EventTimestampParam:
                    result.eventTimestamp = toInt64(field);
                    break;
                case EventResourceParam:
                    result.eventResourceId = QnUuid(field);
                    break;
                case ActionResourceParam:
                    result.actionResourceId = QnUuid(field);
                    break;

                // event specific params.
                case InputPortIdParam:
                    result.inputPortId = QString::fromUtf8(field.data(), field.size());
                    break;

                case ReasonCodeParam:
                    result.reasonCode = (QnBusiness::EventReason) toInt(field);
                    break;
                case ReasonParamsEncodedParam:
                    result.reasonParamsEncoded = QString::fromUtf8(field.data(), field.size());
                    break;

                case SourceParam:
                    result.source = QString::fromUtf8(field.data(), field.size());
                    break;
                case ConflictsParam:
                    result.conflicts = QString::fromLatin1(field.data(), field.size()).split(QLatin1Char(STRING_LIST_DELIM)); // optimization. mac address list here. UTF is not required
                    break;
            default:
                break;
            }
        }

        ++i;
        prevPos = nextPos;
    }

    return result;
}

static void serializeIntParam(QByteArray& result, qint64 value, qint64 defaultValue)
{
    if (value != defaultValue)
        result += QByteArray::number(value);
    result += DELIMITER;
}

static void serializeStringParam(QByteArray& result, const QString& value, const QString& defaultValue)
{
    if (value != defaultValue)
        result += value.toUtf8();
    result += DELIMITER;
}

static void serializeQnIdParam(QByteArray& result, const QnUuid& value)
{
    if (value != QnUuid()) {
        QByteArray data = value.toByteArray();
        result += data;
    }
    result += DELIMITER;
}

void serializeStringListParam(QByteArray& result, const QStringList& value)
{
    if (!value.isEmpty()) {
        for (int i = 0; i < value.size(); ++i) {
            if (i > 0)
                result += STRING_LIST_DELIM;
            result += value[i].toUtf8();
        }
    }
    result += DELIMITER;
}

QByteArray QnBusinessEventParameters::pack() const
{
    QByteArray result;

    static QnBusinessEventParameters defaultParams;

    serializeIntParam(result, eventType, defaultParams.eventType);
    serializeIntParam(result, eventTimestamp, defaultParams.eventTimestamp);
    serializeQnIdParam(result, eventResourceId);
    serializeQnIdParam(result, actionResourceId);
    serializeStringParam(result, inputPortId, defaultParams.inputPortId);
    serializeIntParam(result, reasonCode, defaultParams.reasonCode);
    serializeStringParam(result, reasonParamsEncoded, defaultParams.reasonParamsEncoded);
    serializeStringParam(result, source, defaultParams.source);
    serializeStringListParam(result, conflicts);

    int resLen = result.size();
    for (; resLen > 0 && result.data()[resLen-1] == DELIMITER; --resLen);

    return result.left(resLen);
}

QnUuid QnBusinessEventParameters::getParamsHash() const {
    QByteArray paramKey(QByteArray::number(eventType));
    switch (eventType) {
        case QnBusiness::ServerFailureEvent:
        case QnBusiness::NetworkIssueEvent:
        case QnBusiness::StorageFailureEvent:
            paramKey += '_' + QByteArray::number(reasonCode);
            if (reasonCode == QnBusiness::StorageIoErrorReason 
                || reasonCode == QnBusiness::StorageTooSlowReason 
                || reasonCode == QnBusiness::StorageFullReason 
                || reasonCode == QnBusiness::LicenseRemoved)
                paramKey += '_' + reasonParamsEncoded.toUtf8();
            break;

        case QnBusiness::CameraInputEvent:
            paramKey += '_' + inputPortId.toUtf8();
            break;

        case QnBusiness::CameraDisconnectEvent:
            paramKey += '_' + eventResourceId.toByteArray();
            break;

        default:
            break;
    }
    return guidFromArbitraryData(paramKey);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(QnBusinessEventParameters, (ubjson)(json)(eq), QnBusinessEventParameters_Fields, (optional, true) )
