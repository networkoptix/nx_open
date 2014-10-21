#include "business_event_parameters.h"

#include <QtCore/QVariant>

#include "events/abstract_business_event.h"

#include <utils/common/id.h>

// TODO: #Elric #enum use QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS

static QString PARAM_NAMES[] =
{
    QLatin1String("eventType"),
    QLatin1String("eventTimestamp"),
    QLatin1String("eventResourceId"),
    QLatin1String("actionResourceId"),

    QLatin1String("inputPortId"),

    QLatin1String("reasonCode"),
    QLatin1String("reasonParamsEncoded"),

    QLatin1String("source"),
    QLatin1String("conflicts"),

};

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

QnBusinessParams QnBusinessEventParameters::toBusinessParams() const
{
    static QnBusinessEventParameters defaultParams;
    QnBusinessParams result;

    if (eventType != defaultParams.eventType)
        result.insert(PARAM_NAMES[EventTypeParam], eventType);

    if (eventTimestamp != defaultParams.eventTimestamp)
        result.insert(PARAM_NAMES[EventTimestampParam], eventTimestamp);

    if (eventResourceId != defaultParams.eventResourceId)
        result.insert(PARAM_NAMES[EventResourceParam], eventResourceId.toString());

    if (actionResourceId != defaultParams.actionResourceId)
        result.insert(PARAM_NAMES[ActionResourceParam], actionResourceId.toString());

    if (inputPortId != defaultParams.inputPortId)
        result.insert(PARAM_NAMES[InputPortIdParam], inputPortId);

    if (reasonCode != defaultParams.reasonCode)
        result.insert(PARAM_NAMES[ReasonCodeParam], reasonCode);

    if (reasonParamsEncoded != defaultParams.reasonParamsEncoded)
        result.insert(PARAM_NAMES[ReasonParamsEncodedParam], reasonParamsEncoded);

    if (source != defaultParams.source)
        result.insert(PARAM_NAMES[SourceParam], source);

    if (conflicts != defaultParams.conflicts)
        result.insert(PARAM_NAMES[ConflictsParam], conflicts);

    return result;
}

int QnBusinessEventParameters::getParamIndex(const QString& key)
{
    for (int i = 0; i < (int) ParamCount; ++i)
    {
        if (key == PARAM_NAMES[i])
            return i;
    }
    return -1;
}


QnBusinessEventParameters QnBusinessEventParameters::fromBusinessParams(const QnBusinessParams& bParams)
{
    QnBusinessEventParameters result;

    for (QnBusinessParams::const_iterator itr = bParams.begin(); itr != bParams.end(); ++itr)
    {
        int paramIndex = getParamIndex(itr.key());
        if (paramIndex >= 0) {
            //result.m_params[paramIndex] = itr.value();
            switch (Param( paramIndex))
            {
            case EventTypeParam:
                result.eventType = (QnBusiness::EventType) itr.value().toInt();
                break;
            case EventTimestampParam:
                result.eventTimestamp = itr.value().toLongLong();
                break;
            case EventResourceParam:
                result.eventResourceId = QnUuid(itr.value().toString());
                break;
            case ActionResourceParam:
                result.actionResourceId = QnUuid(itr.value().toString());
                break;

                // event specific params.
            case InputPortIdParam:
                result.inputPortId = itr.value().toString();
                break;

            case ReasonCodeParam:
                result.reasonCode = (QnBusiness::EventReason) itr.value().toInt();
                break;
            case ReasonParamsEncodedParam:
                result.reasonParamsEncoded = itr.value().toString();
                break;

            case SourceParam:
                result.source = itr.value().toString();
                break;
            case ConflictsParam:
                result.conflicts = itr.value().toStringList();
                break;
            default:
                break;
            }
        }
    }

    return result;
}

bool QnBusinessEventParameters::operator==(const QnBusinessEventParameters& other) const
{
    if (eventType != other.eventType)
        return false;
    if (eventTimestamp != other.eventTimestamp)
        return false;
    if (eventResourceId != other.eventResourceId)
        return false;
    if (actionResourceId != other.actionResourceId)
        return false;

    if (inputPortId != other.inputPortId)
        return false;

    if (reasonCode != other.reasonCode)
        return false;
    if (reasonParamsEncoded != other.reasonParamsEncoded)
        return false;
    if (source != other.source)
        return false;
    if (conflicts != other.conflicts)
        return false;

    return true;
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
