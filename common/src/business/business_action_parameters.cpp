#include "business_action_parameters.h"

#include <utils/serialization/json_functions.h>


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
    QLatin1String("sayText"),
};

static const char DELIMITER('$');

QnBusinessActionParameters::QnBusinessActionParameters()
{
    m_userGroup = 0;
    m_fps = 10;
    m_streamQuality = int(Qn::QualityHighest);
    m_recordingDuration = 0;
    m_before = 0;
    m_after = 0;
    m_relayAutoResetTimeout = 0;
}

QString QnBusinessActionParameters::getSoundUrl() const {
    return m_soundUrl;
}

void QnBusinessActionParameters::setSoundUrl(const QString &value) {
    m_soundUrl = value;
}

QString QnBusinessActionParameters::getSayText() const {
    return m_sayText;
}

void QnBusinessActionParameters::setSayText(const QString &value) {
    m_sayText = value;
}

QString QnBusinessActionParameters::getEmailAddress() const {
    return m_emailAddress;
}

void QnBusinessActionParameters::setEmailAddress(const QString &value) {
    m_emailAddress = value;
}

QnBusinessActionParameters::UserGroup QnBusinessActionParameters::getUserGroup() const {
    return (UserGroup) m_userGroup;
}

void QnBusinessActionParameters::setUserGroup(const UserGroup value) {
    m_userGroup = (int)value;
}


int QnBusinessActionParameters::getFps() const {
    return m_fps;
}

void QnBusinessActionParameters::setFps(int value) {
    m_fps = value;
}

Qn::StreamQuality QnBusinessActionParameters::getStreamQuality() const {
    return (Qn::StreamQuality) m_streamQuality;
}

void QnBusinessActionParameters::setStreamQuality(Qn::StreamQuality value) {
    m_streamQuality = (int) value;
}

int QnBusinessActionParameters::getRecordDuration() const {
    return m_recordingDuration;
}

void QnBusinessActionParameters::setRecordDuration(int value) {
    m_recordingDuration = value;
}

int QnBusinessActionParameters::getRecordBefore() const {
    return m_before;
}

void QnBusinessActionParameters::setRecordBefore(int value) {
    m_before = value;
}

int QnBusinessActionParameters::getRecordAfter() const {
    return m_after;
}

void QnBusinessActionParameters::setRecordAfter(int value)  {
    m_after = value;
}

QString QnBusinessActionParameters::getRelayOutputId() const {
    return m_relayOutputID;
}

void QnBusinessActionParameters::setRelayOutputId(const QString &value) {
    m_relayOutputID = value;
}

int QnBusinessActionParameters::getRelayAutoResetTimeout() const {
    return m_relayAutoResetTimeout;
}

void QnBusinessActionParameters::setRelayAutoResetTimeout(int value) {
    m_relayAutoResetTimeout = value;
}

QString QnBusinessActionParameters::getParamsKey() const {
    return m_keyParam;
}

void QnBusinessActionParameters::setParamsKey(QString value) {
    m_keyParam = value;
}


QString QnBusinessActionParameters::getInputPortId() const
{
    return m_inputPortId;
}

void QnBusinessActionParameters::setInputPortId(const QString &value)
{
    m_inputPortId = value;
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

QnBusinessActionParameters QnBusinessActionParameters::deserialize(const QByteArray& value)
{
    QnBusinessActionParameters result;

    if (value.isEmpty())
        return result;

    int i = 0;
    int prevPos = -1;
    while (prevPos < value.size() && i < CountParam)
    {
        int nextPos = value.indexOf(DELIMITER, prevPos+1);
        if (nextPos == -1)
            nextPos = value.size();

        QByteArray field(value.data() + prevPos + 1, nextPos - prevPos - 1);
        switch ((Params) i)
        {
            case soundUrlParam:
                result.m_soundUrl = QString::fromUtf8(field.data(), field.size());
                break;
            case emailAddressParam:
                result.m_emailAddress = QString::fromUtf8(field.data(), field.size());
                break;
            case userGroupParam:
                result.m_userGroup = toInt(field);
                break;
            case fpsParam:
                result.m_fps = toInt(field);
                break;
            case qualityParam:
                result.m_streamQuality = toInt(field);
                break;
            case durationParam:
                result.m_recordingDuration = toInt(field);
                break;
            case beforeParam:
                result.m_before = toInt(field);
                break;
            case afterParam:
                result.m_after = toInt(field);
                break;
            case relayOutputIDParam:
                result.m_relayOutputID = QString::fromUtf8(field.data(), field.size());
                break;
            case relayAutoResetTimeoutParam:
                result.m_relayAutoResetTimeout = toInt(field);
                break;
            case inputPortIdParam:
                result.m_inputPortId = QString::fromUtf8(field.data(), field.size());
                break;
            case keyParam:
                result.m_keyParam = QString::fromUtf8(field.data(), field.size());
                break;
            case sayTextParam:
                result.m_sayText = QString::fromUtf8(field.data(), field.size());
                break;
            default:
                break;
        }
        prevPos = nextPos;
        i++;
    }

    return result;
}

bool QnBusinessActionParameters::isDefault() const
{
    static QnBusinessActionParameters empty;
    return equalTo(empty);
}

bool QnBusinessActionParameters::equalTo(const QnBusinessActionParameters& other) const
{
    if (m_soundUrl != other.m_soundUrl)
        return false;
    if (m_emailAddress != other.m_emailAddress)
        return false;
    if (m_userGroup != other.m_userGroup)
        return false;
    if (m_fps != other.m_fps)
        return false;
    if (m_streamQuality != other.m_streamQuality)
        return false;
    if (m_recordingDuration != other.m_recordingDuration)
        return false;
    if (m_before != other.m_before)
        return false;
    if (m_after != other.m_after)
        return false;
    if (m_relayOutputID != other.m_relayOutputID)
        return false;
    if (m_relayAutoResetTimeout != other.m_relayAutoResetTimeout)
        return false;
    if (m_inputPortId != other.m_inputPortId)
        return false;
    if (m_keyParam != other.m_keyParam)
        return false;
    if (m_sayText != other.m_sayText)
        return false;

    return true;
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

QByteArray QnBusinessActionParameters::serialize() const
{
    QByteArray result;

    static QnBusinessActionParameters m_defaultParams;
    serializeStringParam(result, m_soundUrl, m_defaultParams.m_soundUrl);
    serializeStringParam(result, m_emailAddress, m_defaultParams.m_emailAddress);
    serializeIntParam(result, m_userGroup, m_defaultParams.m_userGroup);
    serializeIntParam(result, m_fps, m_defaultParams.m_fps);
    serializeIntParam(result, m_streamQuality, m_defaultParams.m_streamQuality);
    serializeIntParam(result, m_recordingDuration, m_defaultParams.m_recordingDuration);
    serializeIntParam(result, m_before, m_defaultParams.m_before);
    serializeIntParam(result, m_after, m_defaultParams.m_after);
    serializeStringParam(result, m_relayOutputID, m_defaultParams.m_relayOutputID);
    serializeIntParam(result, m_relayAutoResetTimeout, m_defaultParams.m_relayAutoResetTimeout);
    serializeStringParam(result, m_inputPortId, m_defaultParams.m_inputPortId);
    serializeStringParam(result, m_keyParam, m_defaultParams.m_keyParam);
    serializeStringParam(result, m_sayText, m_defaultParams.m_sayText);

    return result;
}

QnBusinessParams QnBusinessActionParameters::toBusinessParams() const
{
    static QnBusinessActionParameters m_defaultParams;
    QnBusinessParams result;

    if (m_soundUrl != m_defaultParams.m_soundUrl)
        result.insert(PARAM_NAMES[soundUrlParam], m_soundUrl);
    if (m_emailAddress != m_defaultParams.m_emailAddress)
        result.insert(PARAM_NAMES[emailAddressParam], m_emailAddress);
    if (m_userGroup != m_defaultParams.m_userGroup)
        result.insert(PARAM_NAMES[userGroupParam], m_userGroup);
    if (m_fps != m_defaultParams.m_fps)
        result.insert(PARAM_NAMES[fpsParam], m_fps);
    if (m_streamQuality != m_defaultParams.m_streamQuality)
        result.insert(PARAM_NAMES[qualityParam], m_streamQuality);
    if (m_recordingDuration != m_defaultParams.m_recordingDuration)
        result.insert(PARAM_NAMES[durationParam], m_recordingDuration);
    if (m_before != m_defaultParams.m_before)
        result.insert(PARAM_NAMES[beforeParam], m_before);
    if (m_after != m_defaultParams.m_after)
        result.insert(PARAM_NAMES[afterParam], m_after);
    if (m_relayOutputID != m_defaultParams.m_relayOutputID)
        result.insert(PARAM_NAMES[relayOutputIDParam], m_relayOutputID);
    if (m_relayAutoResetTimeout != m_defaultParams.m_relayAutoResetTimeout)
        result.insert(PARAM_NAMES[relayAutoResetTimeoutParam], m_relayAutoResetTimeout);
    if (m_inputPortId != m_defaultParams.m_inputPortId)
        result.insert(PARAM_NAMES[inputPortIdParam], m_inputPortId);
    if (m_keyParam != m_defaultParams.m_keyParam)
        result.insert(PARAM_NAMES[keyParam], m_keyParam);
    if (m_sayText != m_defaultParams.m_sayText)
        result.insert(PARAM_NAMES[sayTextParam], m_sayText);

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
        switch (Params( paramIndex))
        {
        case soundUrlParam:
            result.m_soundUrl = itr.value().toString();
            break;
        case emailAddressParam:
            result.m_emailAddress = itr.value().toString();
            break;
        case userGroupParam:
            result.m_userGroup = itr.value().toInt();
            break;
        case fpsParam:
            result.m_fps = itr.value().toInt();
            break;
        case qualityParam:
            result.m_streamQuality = itr.value().toInt();
            break;
        case durationParam:
            result.m_recordingDuration = itr.value().toInt();
            break;
        case beforeParam:
            result.m_before = itr.value().toInt();
            break;
        case afterParam:
            result.m_after = itr.value().toInt();
            break;
        case relayOutputIDParam:
            result.m_relayOutputID = itr.value().toString();
            break;
        case relayAutoResetTimeoutParam:
            result.m_relayAutoResetTimeout = itr.value().toInt();
            break;
        case inputPortIdParam:
            result.m_inputPortId = itr.value().toString();
            break;
        case keyParam:
            result.m_keyParam = itr.value().toString();
            break;
        case sayTextParam:
            result.m_sayText = itr.value().toString();
            break;
        default:
            break;
        }
    }

    return result;
}

QByteArray serializeBusinessParams(const QnBusinessParams& value) {
    QByteArray result;
    QJson::serialize(QJsonObject::fromVariantMap(value), &result); // TODO: #Elric unuse variants
    return result;
}

QnBusinessParams deserializeBusinessParams(const QByteArray& value) {
    QJsonValue result;
    QJson::deserialize(value, &result); // TODO: #Elric unuse variants
    return result.toObject().toVariantMap(); /* Returns empty map in case of deserialization error. */
}
