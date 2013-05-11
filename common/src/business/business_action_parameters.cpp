#include "business_action_parameters.h"

#include "utils/common/enum_name_mapper.h"


static QLatin1String PARAM_NAMES[] =
{
    QLatin1String("soundUrl"),
    QLatin1String("emailAddress"),
    QLatin1String("userGroup"),
    QLatin1String("fps"),
    QLatin1String("quality"),
    QLatin1String("duration"),
    QLatin1String("before"),
    QLatin1String("after"),
    QLatin1String("relayOutputID"),
    QLatin1String("relayAutoResetTimeout"),
    QLatin1String("inputPortId"),
    QLatin1String("paramsKey"),
};

static const char DELIMITER('$');

QnBusinessActionParameters::QnBusinessActionParameters()
{
    m_params.resize((int) CountParam);
    m_params[userGroupParam] = 0;
    m_params[fpsParam] = 10;
    m_params[qualityParam] = int(QnQualityHighest);
}

QString QnBusinessActionParameters::getSoundUrl() const {
    return m_params[soundUrlParam].toString();
}

void QnBusinessActionParameters::setSoundUrl(const QString &value) {
    m_params[soundUrlParam] = value;
}

QString QnBusinessActionParameters::getEmailAddress() const {
    return m_params[emailAddressParam].toString();
}

void QnBusinessActionParameters::setEmailAddress(const QString &value) {
    m_params[emailAddressParam] = value;
}

QnBusinessActionParameters::UserGroup QnBusinessActionParameters::getUserGroup() const {
    return (UserGroup) m_params[userGroupParam].toInt();
}

void QnBusinessActionParameters::setUserGroup(const UserGroup value) {
    m_params[userGroupParam] = (int)value;
}


int QnBusinessActionParameters::getFps() const {
    return m_params[fpsParam].toInt();
}

void QnBusinessActionParameters::setFps(int value) {
    m_params[fpsParam] = value;
}

QnStreamQuality QnBusinessActionParameters::getStreamQuality() const {
    return (QnStreamQuality) m_params[qualityParam].toInt();
}

void QnBusinessActionParameters::setStreamQuality(QnStreamQuality value) {
    m_params[qualityParam] = (int) value;
}

int QnBusinessActionParameters::getRecordDuration() const {
    return m_params[durationParam].toInt();
}

void QnBusinessActionParameters::setRecordDuration(int value) {
    m_params[durationParam] = value;
}

int QnBusinessActionParameters::getRecordBefore() const {
    return m_params[beforeParam].toInt();
}

void QnBusinessActionParameters::setRecordBefore(int value) {
    m_params[beforeParam] = value;
}

int QnBusinessActionParameters::getRecordAfter() const {
    return m_params[afterParam].toInt();
}

void QnBusinessActionParameters::setRecordAfter(int value)  {
    m_params[afterParam] = value;
}

QString QnBusinessActionParameters::getRelayOutputId() const {
    return m_params[relayOutputIDParam].toString();
}

void QnBusinessActionParameters::setRelayOutputId(const QString &value) {
    m_params[relayOutputIDParam] = value;
}

int QnBusinessActionParameters::getRelayAutoResetTimeout() const {
    return m_params[relayAutoResetTimeoutParam].toInt();
}

void QnBusinessActionParameters::setRelayAutoResetTimeout(int value) {
    m_params[relayAutoResetTimeoutParam] = value;
}

QString QnBusinessActionParameters::getParamsKey() const {
    return m_params[keyParam].toString();
}

void QnBusinessActionParameters::setParamsKey(QString value) {
    m_params[keyParam] = value;
}


QString QnBusinessActionParameters::getInputPortId() const
{
    return m_params[inputPortIdParam].toString();
}

void QnBusinessActionParameters::setInputPortId(const QString &value)
{
    m_params[inputPortIdParam] = value;
}


QnBusinessActionParameters QnBusinessActionParameters::deserialize(const QByteArray& value)
{
    QnBusinessActionParameters result;

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

bool QnBusinessActionParameters::isDefault() const
{
    QnBusinessActionParameters empty;
    for (int i = 0; i < m_params.size(); ++i) {
        if (empty[i] != m_params[i])
            return false;
    }

    return true;
}

QByteArray QnBusinessActionParameters::serialize() const
{
    QByteArray result;
    if (isDefault())
        return QByteArray();
    result = m_params[0].toString().toAscii();
    for (int i = 0; i < m_params.size(); ++i) {
        result += DELIMITER;
        result += m_params[i].toString().toAscii();
    }
    return result;
}

QnBusinessParams QnBusinessActionParameters::toBusinessParams() const
{
    QnBusinessActionParameters empty;

    QnBusinessParams result;
    for (int i = 0; i < (int) CountParam; ++i)
    {
        if (m_params[i] != empty.m_params[i])
            result.insert(PARAM_NAMES[i], m_params[i]);
    }

    return result;
}

int QnBusinessActionParameters::getParamIndex(const QString& key)
{
    for (int i = 0; i < (int) CountParam; ++i)
    {
        if (key == PARAM_NAMES[i])
            return i;
    }
    return -1;
}


QnBusinessActionParameters QnBusinessActionParameters::fromBusinessParams(const QnBusinessParams& bParams)
{
    QnBusinessActionParameters result;

    for (QnBusinessParams::const_iterator itr = bParams.begin(); itr != bParams.end(); ++itr)
    {
        int paramIndex = getParamIndex(itr.key());
        if (paramIndex >= 0)
            result.m_params[paramIndex] = itr.value();
    }

    return result;
}

QVariant& QnBusinessActionParameters::operator[](int index)
{
    return m_params[index];
}

const QVariant& QnBusinessActionParameters::operator[](int index) const
{
    return m_params[index];
}
