#include "business_event_parameters.h"

#include <QtCore/QVariant>

#include "events/abstract_business_event.h"

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
    m_eventType(QnBusiness::UndefinedEvent),
    m_timestamp(0),
    m_reasonCode(QnBusiness::NoReason)
{

}

QnBusiness::EventType QnBusinessEventParameters::getEventType()  const {
    return  m_eventType;
}

void QnBusinessEventParameters::setEventType(QnBusiness::EventType value) {
    m_eventType = value;
}

qint64 QnBusinessEventParameters::getEventTimestamp()  const {
    return m_timestamp;
}

void QnBusinessEventParameters::setEventTimestamp(qint64 value) {
    m_timestamp = value;
}

QnUuid QnBusinessEventParameters::getEventResourceId() const {
    return m_resourceId;
}

void QnBusinessEventParameters::setEventResourceId(const QnUuid& value) {
    m_resourceId = value;
}

QnUuid QnBusinessEventParameters::getActionResourceId() const {
    return m_actionResourceId;
}

void QnBusinessEventParameters::setActionResourceId(const QnUuid& value) {
    m_actionResourceId = value;
}


QnBusiness::EventReason QnBusinessEventParameters::getReasonCode() const{
    return (QnBusiness::EventReason) m_reasonCode;
}

void QnBusinessEventParameters::setReasonCode(QnBusiness::EventReason value) {
    m_reasonCode =  value;
}

QString QnBusinessEventParameters::getReasonParamsEncoded() const {
    return m_reasonParamsEncoded;
}

void QnBusinessEventParameters::setReasonParamsEncoded(const QString& value) {
    m_reasonParamsEncoded = value;
}

QString QnBusinessEventParameters::getSource() const {
    return m_source;
}

void QnBusinessEventParameters::setSource(const QString& value) {
    m_source = value;
}

QStringList QnBusinessEventParameters::getConflicts() const {
    return m_conflicts;
}

void QnBusinessEventParameters::setConflicts(const QStringList& value) {
    m_conflicts = value;
}

QString QnBusinessEventParameters::getInputPortId() const {
    return m_inputPort;
}

void QnBusinessEventParameters::setInputPortId(const QString &value) {
    m_inputPort = value;
}


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
                    result.m_eventType = (QnBusiness::EventType) toInt(field);
                    break;
                case EventTimestampParam:
                    result.m_timestamp = toInt64(field);
                    break;
                case EventResourceParam:
                    result.m_resourceId = QnUuid(field);
                    break;
                case ActionResourceParam:
                    result.m_actionResourceId = QnUuid(field);
                    break;

                // event specific params.
                case InputPortIdParam:
                    result.m_inputPort = QString::fromUtf8(field.data(), field.size());
                    break;

                case ReasonCodeParam:
                    result.m_reasonCode = (QnBusiness::EventReason) toInt(field);
                    break;
                case ReasonParamsEncodedParam:
                    result.m_reasonParamsEncoded = QString::fromUtf8(field.data(), field.size());
                    break;

                case SourceParam:
                    result.m_source = QString::fromUtf8(field.data(), field.size());
                    break;
                case ConflictsParam:
                    result.m_conflicts = QString::fromLatin1(field.data(), field.size()).split(QLatin1Char(STRING_LIST_DELIM)); // optimization. mac address list here. UTF is not required
                    break;
            default:
                break;
            }
        }

        //result.m_params[i++] = QVariant(field);
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

    static QnBusinessEventParameters m_defaultParams;

    serializeIntParam(result, m_eventType, m_defaultParams.m_eventType);
    serializeIntParam(result, m_timestamp, m_defaultParams.m_timestamp);
    serializeQnIdParam(result, m_resourceId);
    serializeQnIdParam(result, m_actionResourceId);
    serializeStringParam(result, m_inputPort, m_defaultParams.m_inputPort);
    serializeIntParam(result, m_reasonCode, m_defaultParams.m_reasonCode);
    serializeStringParam(result, m_reasonParamsEncoded, m_defaultParams.m_reasonParamsEncoded);
    serializeStringParam(result, m_source, m_defaultParams.m_source);
    serializeStringListParam(result, m_conflicts);

    int resLen = result.size();
    for (; resLen > 0 && result.data()[resLen-1] == DELIMITER; --resLen);

    return result.left(resLen);
}

QnBusinessParams QnBusinessEventParameters::toBusinessParams() const
{
    static QnBusinessEventParameters m_defaultParams;
    QnBusinessParams result;

    if (m_eventType != m_defaultParams.m_eventType)
        result.insert(PARAM_NAMES[EventTypeParam], m_eventType);

    if (m_timestamp != m_defaultParams.m_timestamp)
        result.insert(PARAM_NAMES[EventTimestampParam], m_timestamp);

    if (m_resourceId != m_defaultParams.m_resourceId)
        result.insert(PARAM_NAMES[EventResourceParam], m_resourceId.toString());

    if (m_actionResourceId != m_defaultParams.m_actionResourceId)
        result.insert(PARAM_NAMES[ActionResourceParam], m_actionResourceId.toString());

    if (m_inputPort != m_defaultParams.m_inputPort)
        result.insert(PARAM_NAMES[InputPortIdParam], m_inputPort);

    if (m_reasonCode != m_defaultParams.m_reasonCode)
        result.insert(PARAM_NAMES[ReasonCodeParam], m_reasonCode);

    if (m_reasonParamsEncoded != m_defaultParams.m_reasonParamsEncoded)
        result.insert(PARAM_NAMES[ReasonParamsEncodedParam], m_reasonParamsEncoded);

    if (m_source != m_defaultParams.m_source)
        result.insert(PARAM_NAMES[SourceParam], m_source);

    if (m_conflicts != m_defaultParams.m_conflicts)
        result.insert(PARAM_NAMES[ConflictsParam], m_conflicts);

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
                result.m_eventType = (QnBusiness::EventType) itr.value().toInt();
                break;
            case EventTimestampParam:
                result.m_timestamp = itr.value().toLongLong();
                break;
            case EventResourceParam:
                result.m_resourceId = QnUuid(itr.value().toString());
                break;
            case ActionResourceParam:
                result.m_actionResourceId = QnUuid(itr.value().toString());
                break;

                // event specific params.
            case InputPortIdParam:
                result.m_inputPort = itr.value().toString();
                break;

            case ReasonCodeParam:
                result.m_reasonCode = (QnBusiness::EventReason) itr.value().toInt();
                break;
            case ReasonParamsEncodedParam:
                result.m_reasonParamsEncoded = itr.value().toString();
                break;

            case SourceParam:
                result.m_source = itr.value().toString();
                break;
            case ConflictsParam:
                result.m_conflicts = itr.value().toStringList();
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
    if (m_eventType != other.m_eventType)
        return false;
    if (m_timestamp != other.m_timestamp)
        return false;
    if (m_resourceId != other.m_resourceId)
        return false;
    if (m_actionResourceId != other.m_actionResourceId)
        return false;

    if (m_inputPort != other.m_inputPort)
        return false;

    if (m_reasonCode != other.m_reasonCode)
        return false;
    if (m_reasonParamsEncoded != other.m_reasonParamsEncoded)
        return false;
    if (m_source != other.m_source)
        return false;
    if (m_conflicts != other.m_conflicts)
        return false;

    return true;
}

/*
QVariant& QnBusinessEventParameters::operator[](int index)
{
    return m_params[index];
}

const QVariant& QnBusinessEventParameters::operator[](int index) const
{
    return m_params[index];
}
*/

QString QnBusinessEventParameters::getParamsKey() const
{
    QString paramKey(QString::number(getEventType()));

    switch (getEventType())
    {
        case QnBusiness::ServerFailureEvent:
        case QnBusiness::NetworkIssueEvent:
        case QnBusiness::StorageFailureEvent:
            paramKey += QLatin1String("_") + QString::number(getReasonCode());
            if (getReasonCode() == QnBusiness::StorageIoErrorReason || getReasonCode() == QnBusiness::StorageTooSlowReason || getReasonCode() == QnBusiness::StorageFullReason || getReasonCode() == QnBusiness::LicenseRemoved)
                paramKey += QLatin1String("_") + getReasonParamsEncoded();
            break;

        case QnBusiness::CameraInputEvent:
            paramKey += QLatin1String("_") + getInputPortId();
            break;

        case QnBusiness::CameraDisconnectEvent:
            paramKey += QLatin1String("_") + getEventResourceId().toString();
            break;

        default:
            break;
    }

    return paramKey;
}
