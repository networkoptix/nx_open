#include "business_event_parameters.h"

#include "events/abstract_business_event.h"

static QLatin1String PARAM_NAMES[] =
{
    QLatin1String("eventType"),
    QLatin1String("eventTimestamp"),
    QLatin1String("eventResourceId"),
    QLatin1String("actionResourceId"),
    QLatin1String("reasonCode"),
    QLatin1String("reasonText"),
    QLatin1String("source"),
    QLatin1String("conflicts"),
    QLatin1String("paramsKey"),
    QLatin1String("inputPortId"),
    QLatin1String("storageUrl")
};

static const char DELIMITER('$');

QnBusinessEventParameters::QnBusinessEventParameters()
{
    m_params.resize(int (CountParam));

    m_params[eventTypeParam] = (int) BusinessEventType::NotDefined;
}

BusinessEventType::Value QnBusinessEventParameters::getEventType()  const {
    return  (BusinessEventType::Value) m_params[eventTypeParam].toInt();
}

void QnBusinessEventParameters::setEventType(BusinessEventType::Value value) {
    m_params[eventTypeParam] = (int) value;
}

qint64 QnBusinessEventParameters::getEventTimestamp()  const {
    return m_params[eventTimestampParam].toLongLong();
}

void QnBusinessEventParameters::setEventTimestamp(qint64 value) {
    m_params[eventTimestampParam] = value;
}

int QnBusinessEventParameters::getEventResourceId() const {
    return m_params[eventResourceParam].toInt();
}

void QnBusinessEventParameters::setEventResourceId(int value) {
    m_params[eventResourceParam] = value;
}

int QnBusinessEventParameters::getActionResourceId() const {
    return m_params[actionResourceIdParam].toInt();
}

void QnBusinessEventParameters::setActionResourceId(int value) {
    m_params[actionResourceIdParam] = value;
}



QnBusiness::EventReason QnBusinessEventParameters::getReasonCode() const{
    return (QnBusiness::EventReason) m_params[reasonCodeParam].toInt();
}

void QnBusinessEventParameters::setReasonCode(QnBusiness::EventReason value) {
    m_params[reasonCodeParam] = (int) value;
}

QString QnBusinessEventParameters::getReasonText() const {
    return m_params[reasonTextParam].toString();
}

void QnBusinessEventParameters::setReasonText(const QString& value) {
    m_params[reasonTextParam] = value;
}

QString QnBusinessEventParameters::getSource() const {
    return m_params[sourceParam].toString();
}

void QnBusinessEventParameters::setSource(QString value) {
    m_params[sourceParam] = value;
}

QStringList QnBusinessEventParameters::getConflicts() const {
    return m_params[conflictsParam].value<QStringList>();
}

void QnBusinessEventParameters::setConflicts(QStringList value) {
    m_params[conflictsParam] = QVariant::fromValue(value);
}


QString QnBusinessEventParameters::getParamsKey() const {
    return m_params[keyParam].toString();
}

void QnBusinessEventParameters::setParamsKey(QString value) {
    m_params[keyParam] = value;
}

QString QnBusinessEventParameters::getInputPortId() const {
    return m_params[inputPortIdParam].toString();
}

void QnBusinessEventParameters::setInputPortId(const QString &value) {
    m_params[inputPortIdParam] = value;
}

QString QnBusinessEventParameters::getStorageResourceUrl() const
{
    return m_params[storageUrlParam].toString();
}

void QnBusinessEventParameters::setStorageResourceUrl(QString value)
{
    m_params[storageUrlParam] = value;
}


QnBusinessEventParameters QnBusinessEventParameters::deserialize(const QByteArray& value)
{
    QnBusinessEventParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = value.indexOf(DELIMITER);
    while (prevPos < value.size() && i < CountParam)
    {
        int nextPos = value.indexOf(prevPos, DELIMITER);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field = QByteArray::fromRawData(value.data() + prevPos, nextPos - prevPos);
        result.m_params[i++] = QVariant(field);
        prevPos = nextPos;
    }

    return result;
}

QByteArray QnBusinessEventParameters::serialize() const
{
    QByteArray result;
    result = m_params[0].toString().toAscii();
    for (int i = 0; i < m_params.size(); ++i) {
        result += DELIMITER;
        result += m_params[i].toString().toAscii();
    }
    return result;
}

QnBusinessParams QnBusinessEventParameters::toBusinessParams() const
{
    QnBusinessEventParameters empty;

    QnBusinessParams result;
    for (int i = 0; i < (int) CountParam; ++i)
    {
        if (m_params[i] != empty.m_params[i])
            result.insert(PARAM_NAMES[i], m_params[i]);
    }

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
        if (paramIndex >= 0)
            result.m_params[paramIndex] = itr.value();
    }

    return result;
}

QVariant& QnBusinessEventParameters::operator[](int index)
{
    return m_params[index];
}

const QVariant& QnBusinessEventParameters::operator[](int index) const
{
    return m_params[index];
}
