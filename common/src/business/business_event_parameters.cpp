#include "business_event_parameters.h"

#include "events/abstract_business_event.h"

static QLatin1String PARAM_NAMES[] =
{
    QLatin1String("eventType"),
    QLatin1String("eventTimestamp"),
    QLatin1String("eventResourceId"),
    QLatin1String("actionResourceId"),

    QLatin1String("inputPortId"),

    QLatin1String("reasonCode"),
    QLatin1String("reasonText"),

    QLatin1String("source"),
    QLatin1String("conflicts"),

};

static const char DELIMITER('$');
static const char STRING_LIST_DELIM('\n');

QnBusinessEventParameters::QnBusinessEventParameters():
    m_eventType(BusinessEventType::NotDefined),
    m_timestamp(0),
    m_reasonCode(QnBusiness::NoReason)
{

}

BusinessEventType::Value QnBusinessEventParameters::getEventType()  const {
    return  m_eventType;
}

void QnBusinessEventParameters::setEventType(BusinessEventType::Value value) {
    m_eventType = value;
}

qint64 QnBusinessEventParameters::getEventTimestamp()  const {
    return m_timestamp;
}

void QnBusinessEventParameters::setEventTimestamp(qint64 value) {
    m_timestamp = value;
}

int QnBusinessEventParameters::getEventResourceId() const {
    return m_resourceId;
}

void QnBusinessEventParameters::setEventResourceId(int value) {
    m_resourceId = value;
}

int QnBusinessEventParameters::getActionResourceId() const {
    return m_actionResourceId;
}

void QnBusinessEventParameters::setActionResourceId(int value) {
    m_actionResourceId = value;
}


QnBusiness::EventReason QnBusinessEventParameters::getReasonCode() const{
    return (QnBusiness::EventReason) m_reasonCode;
}

void QnBusinessEventParameters::setReasonCode(QnBusiness::EventReason value) {
    m_reasonCode =  value;
}

QString QnBusinessEventParameters::getReasonText() const {
    return m_reasonText;
}

void QnBusinessEventParameters::setReasonText(const QString& value) {
    m_reasonText = value;
}

QByteArray QnBusinessEventParameters::getSource() const {
    return m_source;
}

void QnBusinessEventParameters::setSource(const QByteArray& value) {
    m_source = value;
}

QList<QByteArray> QnBusinessEventParameters::getConflicts() const {
    return m_conflicts;
}

void QnBusinessEventParameters::setConflicts(const QList<QByteArray>& value) {
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

QnBusinessEventParameters QnBusinessEventParameters::deserialize(const QByteArray& value)
{
    QnBusinessEventParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < CountParam)
    {
        int nextPos = value.indexOf(DELIMITER, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field = QByteArray::fromRawData(value.data() + prevPos + 1, nextPos - prevPos - 1);
        if (!field.isEmpty())
        {
            switch ((Params) i)
            {
                case eventTypeParam:
                    result.m_eventType = (BusinessEventType::Value) toInt(field);
                    break;
                case eventTimestampParam:
                    result.m_timestamp = toInt64(field);
                    break;
                case eventResourceParam:
                    result.m_resourceId = toInt(field);
                    break;
                case actionResourceParam:
                    result.m_actionResourceId = toInt(field);
                    break;

                // event specific params.
                case inputPortIdParam:
                    result.m_inputPort = QString::fromUtf8(field);
                    break;

                case reasonCodeParam:
                    result.m_reasonCode = (QnBusiness::EventReason) toInt(field);
                    break;
                case reasonTextParam:
                    result.m_reasonText = QString::fromUtf8(field);
                    break;

                case sourceParam:
                    result.m_source = field;
                    break;
                case conflictsParam:
                    result.m_conflicts = field.split(STRING_LIST_DELIM);
                    break;
            }
        }

        //result.m_params[i++] = QVariant(field);
        ++i;
        prevPos = nextPos;
    }

    return result;
}

void serializeIntParam(QByteArray& result, qint64 value, qint64 defaultValue)
{
    if (value != defaultValue)
        result += QByteArray::number(value);
    result += DELIMITER;
}

void serializeStringParam(QByteArray& result, const QString& value, const QString& defaultValue)
{
    if (value != defaultValue)
        result += value.toUtf8();
    result += DELIMITER;
}

void serializeByteArrayParam(QByteArray& result, const QByteArray& value, const QByteArray& defaultValue)
{
    if (value != defaultValue)
        result += value;
    result += DELIMITER;
}

void serializeByteArrayListParam(QByteArray& result, const QList<QByteArray>& value)
{
    if (!value.isEmpty()) {
        for (int i = 0; i < value.size(); ++i) {
            if (i > 0)
                result += STRING_LIST_DELIM;
            result += value[i];
        }
    }
    result += DELIMITER;
}

QByteArray QnBusinessEventParameters::serialize() const
{
    QByteArray result;

    static QnBusinessEventParameters m_defaultParams;

    serializeIntParam(result, m_eventType, m_defaultParams.m_eventType);
    serializeIntParam(result, m_timestamp, m_defaultParams.m_timestamp);
    serializeIntParam(result, m_resourceId, m_defaultParams.m_resourceId);
    serializeIntParam(result, m_actionResourceId, m_defaultParams.m_actionResourceId);
    serializeStringParam(result, m_inputPort, m_defaultParams.m_inputPort);
    serializeIntParam(result, m_reasonCode, m_defaultParams.m_reasonCode);
    serializeStringParam(result, m_reasonText, m_defaultParams.m_reasonText);
    serializeByteArrayParam(result, m_source, m_defaultParams.m_source);
    serializeByteArrayListParam(result, m_conflicts);

    int resLen = result.size();
    for (; resLen > 0 && result.data()[resLen-1] == DELIMITER; --resLen);

    return result.left(resLen);
}

static QList<QByteArray> baFromStringList(const QStringList& values)
{
    QList<QByteArray> result;
    for (int i = 0; i < values.size(); ++i)
        result << values[i].toUtf8();

    return result;
}

static QStringList baToStringList(const QList<QByteArray>& values)
{
    QStringList result;
    for (int i = 0; i < values.size(); ++i)
        result << QString::fromUtf8(values[i]);

    return result;
}

QnBusinessParams QnBusinessEventParameters::toBusinessParams() const
{
    static QnBusinessEventParameters m_defaultParams;
    QnBusinessParams result;

    if (m_eventType != m_defaultParams.m_eventType)
        result.insert(PARAM_NAMES[eventTypeParam], m_eventType);

    if (m_timestamp != m_defaultParams.m_timestamp)
        result.insert(PARAM_NAMES[eventTimestampParam], m_timestamp);

    if (m_resourceId != m_defaultParams.m_resourceId)
        result.insert(PARAM_NAMES[eventResourceParam], m_resourceId.toInt());

    if (m_actionResourceId != m_defaultParams.m_actionResourceId)
        result.insert(PARAM_NAMES[actionResourceParam], m_actionResourceId.toInt());

    if (m_inputPort != m_defaultParams.m_inputPort)
        result.insert(PARAM_NAMES[inputPortIdParam], m_inputPort);

    if (m_reasonCode != m_defaultParams.m_reasonCode)
        result.insert(PARAM_NAMES[reasonCodeParam], m_reasonCode);

    if (m_reasonText != m_defaultParams.m_reasonText)
        result.insert(PARAM_NAMES[reasonTextParam], m_reasonText);

    if (m_source != m_defaultParams.m_source)
        result.insert(PARAM_NAMES[sourceParam], m_source);

    if (m_conflicts != m_defaultParams.m_conflicts)
        result.insert(PARAM_NAMES[conflictsParam], baToStringList(m_conflicts));

    /*
    for (int i = 0; i < (int) CountParam; ++i)
    {
        if (m_params[i] != empty.m_params[i])
            result.insert(PARAM_NAMES[i], m_params[i]);
    }
    */

    return result;
}

int QnBusinessEventParameters::getParamIndex(const QString& key)
{
    for (int i = 0; i < (int) CountParam; ++i)
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
            switch (Params( paramIndex))
            {
            case eventTypeParam:
                result.m_eventType = (BusinessEventType::Value) itr.value().toInt();
                break;
            case eventTimestampParam:
                result.m_timestamp = itr.value().toLongLong();
                break;
            case eventResourceParam:
                result.m_resourceId = itr.value().toInt();
                break;
            case actionResourceParam:
                result.m_actionResourceId = itr.value().toInt();
                break;

                // event specific params.
            case inputPortIdParam:
                result.m_inputPort = itr.value().toString();
                break;

            case reasonCodeParam:
                result.m_reasonCode = (QnBusiness::EventReason) itr.value().toInt();
                break;
            case reasonTextParam:
                result.m_reasonText = itr.value().toString();
                break;

            case sourceParam:
                result.m_source = itr.value().toString().toUtf8();
                break;
            case conflictsParam:
                result.m_conflicts = baFromStringList(itr.value().toStringList());
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
    if (m_reasonText != other.m_reasonText)
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
        case BusinessEventType::MediaServer_Failure:
        case BusinessEventType::Network_Issue:
        case BusinessEventType::Storage_Failure:
            paramKey += QLatin1String("_") + QString::number(getReasonCode());
            if (getReasonCode() == QnBusiness::StorageIssueIoError || getReasonCode() == QnBusiness::StorageIssueNotEnoughSpeed)
                paramKey += QLatin1String("_") + getReasonText();
            break;
        case BusinessEventType::Camera_Input:
            paramKey += QLatin1String("_") + getInputPortId();
            break;
    }

    return paramKey;
}
